#include "condor_common.h"
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_utility.h"
#include "condor_string.h"
#include "condor_config.h"
#include "env.h"
#include "setenv.h"
#include <unistd.h>
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
	if( !envStrParser.Merge(env) ) {
		// this is an invalid env string
		fprintf(stderr, "Warning! Configuration file variable "
				"`%s' has invalid value `%s'; ignoring.\n",
				STORK_ENVIRONMENT, env);
	} else {
		char **unix_env;	// TODO: delete when done?
		unix_env = envStrParser.getStringArray();
		for ( int j=0 ; (unix_env[j] && unix_env[j][0]) ; j++ ) {
			if ( !SetEnv( unix_env[j] ) ) {
				dprintf ( D_ALWAYS, "Failed to put "
						  "\"%s\" into environment.\n", unix_env[j] );
			  free(env);
				return DAP_ERROR;
			}
		}
	}
	  free(env);
  }

  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  strncpy(arguments, "", MAXSTR);
  
  for(i=3;i<argc;i++) {
    size_t len = strlen(arguments);
    if(len) {
      strncpy(arguments+len, " ", MAXSTR-len);
      len++;
    }
    strncpy(arguments+len, argv[i], MAXSTR-len); 
  }
  
  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);

  fprintf(stdout, "Transfering from: %s to: %s with arguments: %s\n", 
	  src_url, dest_url, arguments);

  status = transfer_globus_url_copy(src_url, dest_url, arguments, error_str); 
  

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = fopen(fname, "w");
    fprintf(f, "GLOBUS error: %s", error_str);
    fclose(f);  

    return DAP_ERROR;
  }
  //--


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














