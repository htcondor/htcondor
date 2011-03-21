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
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_utility.h"
#include "condor_string.h"
#include "condor_config.h"
#include "env.h"
#include "setenv.h"
#include <unistd.h>
#include "my_hostname.h"
#include <string>

#include "globus_ftp_client.h"


static globus_mutex_t glock;
static globus_cond_t cond;
static globus_bool_t done;
static globus_bool_t error = GLOBUS_FALSE;

#define SIZE 42
#define SIZE_UNKNOWN	( (unsigned long)(-1) )
#define GLOBUS_URL_COPY	"globus-url-copy"

/* ========================================================================== */
static void done_cb(void *user_arg, 
		    globus_ftp_client_handle_t *handle,
		    globus_object_t *err)
{
  char * tmpstr;

  if(err)
    {
      tmpstr = globus_object_printable_to_string(err);
      fprintf(stderr, "%s\n", tmpstr);
      error = GLOBUS_TRUE;
      globus_libc_free(tmpstr);
    }
  globus_mutex_lock(&glock);
  done = GLOBUS_TRUE;
  globus_cond_signal(&cond);
  globus_mutex_unlock(&glock);
}
/* ========================================================================== */
int get_filesize_from_gridftp(char *src_url, unsigned long &filesize)
{
  globus_ftp_client_handle_t                  handle;
  globus_ftp_client_operationattr_t           attr;
  globus_ftp_client_handleattr_t              handle_attr;
  globus_result_t                             result;
  globus_off_t                                size;

  globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
  globus_ftp_client_handleattr_init(&handle_attr);
  globus_ftp_client_operationattr_init(&attr);

  globus_mutex_init(&glock, GLOBUS_NULL);
  globus_cond_init(&cond, GLOBUS_NULL);
  globus_ftp_client_handle_init(&handle, &handle_attr);
  done = GLOBUS_FALSE;
  
  result = globus_ftp_client_size(&handle,
				  src_url,
				  &attr,
				  &size,
				  done_cb,
				  0);
  
  if(result != GLOBUS_SUCCESS){
    fprintf(stderr, globus_object_printable_to_string(globus_error_get(result)));
    error = GLOBUS_TRUE;
    done = GLOBUS_TRUE;
  }
  globus_mutex_lock(&glock);
  while(!done){
    globus_cond_wait(&cond, &glock);
  }
  globus_mutex_unlock(&glock);
  globus_ftp_client_handle_destroy(&handle);
  globus_module_deactivate_all();

  if(error != GLOBUS_SUCCESS){
    return DAP_ERROR;
  }
  else{
    //    printf("SRC Filesize = %"GLOBUS_OFF_T_FORMAT"\n", size);
    filesize = size;
    return DAP_SUCCESS;
  }
  
}

/* ========================================================================== */
int transfer_globus_url_copy(char *src_url, char *dest_url, 
			     char *arguments, char *error_str)
{
  FILE *pipe = 0;
  char pipecommand[MAXSTR];
  char linebuf[MAXSTR] = "";
  int ret;

  snprintf(pipecommand, MAXSTR, "%s %s %s %s 2>&1", 
  	   GLOBUS_URL_COPY, arguments, src_url, dest_url);

  //  "-tcp-bs 208896 -bs 1048576 -p 4"

  pipe = popen (pipecommand, "r");

  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s!\n",
			GLOBUS_URL_COPY);
    return -1;
  }

  
  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }
   
    fprintf(stdout, ">%s", linebuf);
  }
   
  ret = pclose(pipe);
  fprintf(stdout, "Command terminated with err_code: %d\n", ret);

  if (ret == 0)
    return DAP_SUCCESS;
  else {
    strncpy(error_str, linebuf, MAXSTR);
    return DAP_ERROR;
  }
  
}

const char *
unique_filepath(const char *directory)
{
	static char path[_POSIX_PATH_MAX];
	static int unique = 0;
	static time_t curr = 0;

	if (unique) {
		unique++;
	} else {
		unique = 1;
		curr = time(NULL);
	}

	sprintf(path, "%s/file-%u-%d-%d-%d", directory, my_ip_addr(), getpid(),
			(int)curr, unique);
	return path;
}

// For multi-file transfers using a dynamic transfer destination, the list
// specifying the file transfer source-destination pairs must be rewritten:
// all destination URLs must be rewritten with the dynamic destination.
bool
translate_file(
	const char *multi_file_xfer_file,	// original static dest url file
	const char *dynamic_multi_file_xfer_file,	// new dynamic dest url file
	const char *dynamic_destination		// dynamic dest url to use
)
{
	char src[MAXSTR], dest[MAXSTR];
	FILE *static_urls = fopen(multi_file_xfer_file, "r");
	if (! static_urls) {
		fprintf(stderr, "open static URL specification file %s for read: %s\n",
				multi_file_xfer_file, strerror(errno) );
		return false;
	}
	FILE *dynamic_urls = fopen(dynamic_multi_file_xfer_file, "w");
	if (! dynamic_urls) {
		fprintf(stderr,"open dynamic URL specification file %s for write: %s\n",
				dynamic_multi_file_xfer_file, strerror(errno) );
		fclose(static_urls);
		return false;
	}

	while (fscanf(static_urls, " %s %s ", src, dest) != EOF) {
		fprintf(dynamic_urls, "%s %s\n",
				src, unique_filepath(dynamic_destination) );
	}
	fclose(static_urls);
	fclose(dynamic_urls);
	return true;
}

/* ========================================================================== */
int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
  char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
  char error_str[MAXSTR];
  unsigned long src_filesize, dest_filesize;
  int status;
  struct stat filestat;
  int i;
  bool multi_file_xfer = false;
  char * multi_file_xfer_file = NULL;
  const char *dynamic_multi_file_xfer_file = NULL;
  bool dynamic_file_xfer = false;
  
  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url> [<arguments>]\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
   }

    config(); // read config file

  // TODO: The correct parameter name is actually daemonname_ENVIRONMENT, where
  // daemonname is the Stork daemon name, known to the Condor Master, and
  // defined in the DAEMON_LIST parameter.  The daemonname is _usually_ defined
  // to be STORK by convention, but can be anything.
#define STORK_ENVIRONMENT		"STORK_ENVIRONMENT"
  char* env = param(STORK_ENVIRONMENT);
  if (env) {
	  printf("merging environment defined by %s configuration\n",
			  STORK_ENVIRONMENT);
	  Env envStrParser;
	// Note: If [name]_ENVIRONMENT is not specified, env will now be null.
	// Env::Merge(null) will always return true, so the warning will not be
	// printed in this case.
    MyString env_errors;
	if( !envStrParser.MergeFromV1RawOrV2Quoted(env,&env_errors) ) {
		// this is an invalid env string
		fprintf(stderr, "Warning! Configuration file variable "
				"`%s' has invalid value `%s'; ignoring: %s\n",
				STORK_ENVIRONMENT, env, env_errors.Value());
		free(env);
	} else {
		char **unix_env;
		free(env);
		unix_env = envStrParser.getStringArray();
		for ( int j=0 ; (unix_env[j] && unix_env[j][0]) ; j++ ) {
			if ( !SetEnv( unix_env[j] ) ) {
				dprintf ( D_ALWAYS, "Failed to put "
						  "\"%s\" into environment.\n", unix_env[j] );
				return DAP_ERROR;
			}
		}
		deleteStringArray(unix_env);
	}
  }

  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  strncpy(arguments, "", MAXSTR);
  
  for(i=3;i<argc;i++) {
	if (! strcmp( argv[i], "-dynamic") ) {
		// FIXME: Find a better IPC to communicate this.
		// intercept special "-dynamic" option, not for globus-url-copy

		// WARNING: the "-dynamic option must appear before any "-f" option to
		// specify a multiple transfer file list.
		dynamic_file_xfer = true;
		continue;	// Do not pass this option to globus-url-copy
	} else if (! strcmp( argv[i], "-f") ) {
		multi_file_xfer = true;
	} else if (multi_file_xfer == true && ! multi_file_xfer_file) {
		multi_file_xfer_file = argv[i];
		if (dynamic_file_xfer) {
			// For dynamic transfer destinations, the multi file transfer list
			// specification file needs to be rewritten.  Ugh.
			dynamic_multi_file_xfer_file =
				job_filepath("", "urls",
						getenv("STORK_JOBID"), getpid() );
			argv[i] = (char *)dynamic_multi_file_xfer_file;
		}
	}
    size_t len = strlen(arguments);
    if(len) {
      strncpy(arguments+len, " ", MAXSTR-len);
      len++;
    }
    strncpy(arguments+len, argv[i], MAXSTR-len); 
  }
  if (multi_file_xfer == true && ! multi_file_xfer_file) {
    fprintf(stderr, "command \"%s\": no multiple transfer file found\n",
		arguments);
    return DAP_ERROR;
  }
  
  // Horrid hack to enable multi-file xfers.  If "-f" option appears as an
  // argument, then remove src_url, dest_url.
  if (multi_file_xfer) {
	  if (dynamic_multi_file_xfer_file) {
		  if (! translate_file(multi_file_xfer_file,
					  dynamic_multi_file_xfer_file, dest_url) ) {
			return DAP_ERROR;
		  }
	  }
      fprintf(stdout,
              "Multi-file transferring from: %s to: %s with arguments: %s\n", 
          src_url, dest_url, arguments);
      strcpy(src_url, "");
      strcpy(dest_url, "");
  } else {
      parse_url(src_url, src_protocol, src_host, src_file);
      parse_url(dest_url, dest_protocol, dest_host, dest_file);

      fprintf(stdout, "Transfering from: %s to: %s with arguments: %s\n", 
          src_url, dest_url, arguments);
  }

  status = transfer_globus_url_copy(src_url, dest_url, arguments, error_str); 
  

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "GLOBUS error: %s", error_str);
    fclose(f);  

    return DAP_ERROR;
  }
  //--

    if ( multi_file_xfer ) {
		fprintf(stdout, "skipping file size check for multi-file xfer\n");
        fprintf(stderr, "SUCCESS!\n");
        return DAP_SUCCESS;
    }

  /*
  //return without double checking success by comparing src and dest file sizes
  if (status == DAP_SUCCESS){
    return DAP_SUCCESS;
  }
  */

  src_filesize = SIZE_UNKNOWN;
  dest_filesize = SIZE_UNKNOWN;

	//double check success by comparing src and dest file sizes
	// TODO: How do we check the filesize for http???
	// source URL
	if (!strcmp(src_protocol, "file")){
		if (!stat(src_file, &filestat)){  //if file exists
			src_filesize = filestat.st_size;
      }
      else{
		return DAP_ERROR;
      }
    }
    else{
		// Only check get_filesize_from_gridftp if protocol =[gsi]ftp:
		if (!strcmp(src_protocol, "ftp") || !strcmp(src_protocol, "gsiftp") ) {
			status = get_filesize_from_gridftp(src_url, src_filesize);
			if ( status != DAP_SUCCESS ) {
				return DAP_ERROR;
			}
		}
	}

	// destination URL
	if (!strcmp(dest_protocol, "file")){
		if (!stat(dest_file, &filestat)){  //if file exists
			dest_filesize = filestat.st_size;
      }
      else{
		return DAP_ERROR;
      }
    }
    else{
		// Only check get_filesize_from_gridftp if protocol =[gsi]ftp:
		if (!strcmp(dest_protocol, "ftp") || !strcmp(dest_protocol,"gsiftp") ) {
			status = get_filesize_from_gridftp(dest_url, dest_filesize);
			if ( status != DAP_SUCCESS ) {
				return DAP_ERROR;
			}
		}
	}

	if ( src_filesize == SIZE_UNKNOWN ) {
		fprintf(stdout, "size of  src (%s) = unknown\n", src_url);

	} else {
		fprintf(stdout, "size of  src (%s) = %ld\n", src_url, src_filesize);
	}

	if ( dest_filesize == SIZE_UNKNOWN ) {
		fprintf(stdout, "size of dest (%s) = unknown\n", dest_url);

	} else {
		fprintf(stdout, "size of dest (%s) = %ld\n", dest_url, dest_filesize);
	}

	if ( src_filesize == SIZE_UNKNOWN || dest_filesize == SIZE_UNKNOWN) {
		fprintf(stderr, "SUCCESS\n");
		return DAP_SUCCESS;
	}

	if (src_filesize != dest_filesize) {
		fprintf(stderr, "Error: src and dest file sizes do not match!\n");
		return DAP_ERROR;
    }
	fprintf(stderr, "SUCCESS!\n");
	return DAP_SUCCESS;
}














