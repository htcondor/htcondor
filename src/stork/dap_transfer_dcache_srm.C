#include "condor_common.h"
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_utility.h"
#include "dap_classad_reader.h"
#include <unistd.h>

//default value for DCACHE_SRM_BIN_DIR, will be overwritten by stork.config
char DCACHE_SRM_BIN_DIR[MAXSTR] = "/opt/d-cache-client/srm/bin";

/* ==========================================================================
 * read the stork config file to get DCACHE_SRM_BIN_DIR
 * ==========================================================================*/
void read_config_file()
{
  char dcache_srm_bin_dir[MAXSTR];

  char * STORK_CONFIG_FILE = getenv ("STORK_CONFIG_FILE");
  if (STORK_CONFIG_FILE == NULL) {
    fprintf (stderr, "ERROR: STORK_CONFIG_FILE not set!\n");
    exit (1);
  }

  ClassAd_Reader configreader(STORK_CONFIG_FILE);
  
  if ( !configreader.readAd()) {
    printf("ERROR in parsing the Stork Config file: %s\n", STORK_CONFIG_FILE);    
    return;
  }

  //get value for DCACHE_SRM_BIN_DIR
  if (configreader.getValue("dcache_srm_bin_dir", dcache_srm_bin_dir) == DAP_SUCCESS){
    strncpy(DCACHE_SRM_BIN_DIR, strip_str(dcache_srm_bin_dir), MAXSTR);
  }

  printf("dcache_srm_bin_dir = %s\n", DCACHE_SRM_BIN_DIR);  
}

int transfer_from_to_dcache_srm(char *src_url, char *dest_url, 
			     char *arguments, char *error_str)
{
  FILE *pipe = 0;
  char pipecommand[MAXSTR], executable[MAXSTR];
  char linebuf[MAXSTR];
  int ret;
  struct stat filestat;

  snprintf(executable, MAXSTR, "%s/srmcp", DCACHE_SRM_BIN_DIR);

  if (lstat(executable, &filestat)){  //if file does not exists
    fprintf(stderr, "Executable %s not found!\n", executable);
    return DAP_ERROR;
  }

  // Stork server should set X509_USER_PROXY env variable when applicable
  MyString x509proxy;
  char * X509_USER_PROXY = getenv ("X509_USER_PROXY");
  if (X509_USER_PROXY != NULL) {
    x509proxy += " -x509_user_proxy=";
    x509proxy += X509_USER_PROXY;
    free (X509_USER_PROXY);
  }

  snprintf(pipecommand, MAXSTR, "%s %s %s %s %s", 
	   executable, 
	   arguments, 
	   src_url, 
	   dest_url,
	   x509proxy.Value());

  pipe = popen (pipecommand, "r");

  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to srmcp!\n");
    return -1;
  }

  /*
  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }
    if (sscanf(linebuf, "copying ... %s", status ) > 0){
      if (!strcmp(status, "done")){
	return DAP_SUCCESS;
      }
    }
    
    fprintf(stdout, ">%s", linebuf);
  }
  */

  ret = pclose(pipe);
  fprintf(stdout, "Cammand terminated with err_code: %d\n", ret);

  if (ret == 0)
    return DAP_SUCCESS;
  else {
    strncpy(error_str, linebuf, MAXSTR);
    return DAP_ERROR;
  }

  /*
  pclose(pipe);
  strncpy(error_str, linebuf, MAXSTR);
  return DAP_ERROR;
  */

}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
  char error_str[MAXSTR];
  int status;
  
  if (argc < 3){
    fprintf(stdout, "==============================================================\n");
    fprintf(stdout, "USAGE: %s [<arguments>] <src_url> <dest_url> \n", argv[0]);
    fprintf(stdout, "==============================================================\n");
    exit(-1);
  }

  read_config_file();
  
  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  strncpy(arguments, "", MAXSTR);
  for(int i=3;i<argc;i++) {
    size_t len = strlen(arguments);
    if(len) {
      strncpy(arguments+len, " ", MAXSTR-len);
      len++;
    }
    strncpy(arguments+len, argv[i], MAXSTR-len); 
  }

  fprintf(stdout, "Transfering from: %s to: %s with arguments: %s\n", 
	  src_url, dest_url, arguments);

  status = transfer_from_to_dcache_srm(src_url, dest_url, arguments, error_str); 


  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = fopen(fname, "w");
    fprintf(f, "DCACHE SRM error: %s", error_str);
    fclose(f);  
  }
  //--

  return status;
}







