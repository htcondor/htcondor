#include "dap_constants.h"
#include "condor_common.h"
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_config.h"
#include "globus_utils.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

#define USAGE \
"[stork_server] submit_file\n\
stork_server\t\t\tspecify explicit stork server (deprecated)\n\
submit_file\t\t\tstork submit file\n\
\t-lognotes \"notes\"\tadd lognote to submit file before processing\n\
\t-stdin\t\t\tread submission from stdin instead of a file"

int check_dap_format(classad::ClassAd *currentAd)
{
  char dap_type[MAXSTR];
  std::string adbuffer = "";
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;

  //should be context insensitive..
  //just check the format, not the content!
  //except dap_type ??
  
  if ( !(attrexpr = currentAd->Lookup("dap_type")) ) return 0;
  else{
    unparser.Unparse(adbuffer,attrexpr);

    char tmp_adbuffer[MAXSTR];
    strncpy(tmp_adbuffer, adbuffer.c_str(), MAXSTR);
    strcpy(dap_type,strip_str(tmp_adbuffer));

    if ( !strcmp(dap_type, "transfer") ){
      if ( !currentAd->Lookup("src_url") )  return 0;
      if ( !currentAd->Lookup("dest_url") ) return 0;
    }
    
    else if ( !strcmp(dap_type, "reserve") ){
      if ( !currentAd->Lookup("dest_host") )    return 0;
      //if ( !currentAd->Lookup("reserve_size") ) return 0;
      //if ( !currentAd->Lookup("duration") )     return 0;
      //if ( !currentAd->Lookup("reserve_id") )   return 0;
    }

    else if ( !strcmp(dap_type, "release") ){
      if ( !currentAd->Lookup("dest_host") ) return 0;
      if ( !currentAd->Lookup("reserve_id") ) return 0;
    }
    
    else return 0;
  }
  
  
  return 1;
}
//============================================================================
void MissingArgument(char *argv0,char *arg)
{
  fprintf(stderr,"Missing argument: %s\n",arg);
  stork_print_usage(stderr, argv0, USAGE, true);
  exit(1);
}

void IllegalOption(char *argv0,char *arg)
{
  fprintf(stderr,"Illegal option: %s\n",arg);
  stork_print_usage(stderr, argv0, USAGE, true);
  exit(1);
}

/* ============================================================================
 * main body of dap_submit
 * ==========================================================================*/
int main(int argc, char **argv)
{
  int i; char c;
  std::string adstr="";
  classad::ClassAdParser parser;
  classad::ClassAd *currentAd = NULL;
  int leftparan = 0;
  FILE *adfile;
  char fname[_POSIX_PATH_MAX];
  char *lognotes = NULL;
  int read_from_stdin = 0;
  struct stork_global_opts global_opts;

  config();	// read config file

  // Parse out stork global options.
  stork_parse_global_opts(argc, argv, USAGE, &global_opts, true);

  for(i=1;i<argc;i++) {
    char *arg = argv[i];
    if(arg[0] != '-') {
      break; //this must be a positional argument
    }
#define OPT_LOGNOTES	"-l"	// -lognotes
    else if(!strncmp(arg,OPT_LOGNOTES,strlen(OPT_LOGNOTES) ) ) {
      if(i+1 >= argc) MissingArgument(argv[0],arg);
      lognotes = argv[++i];
    }
#define OPT_STDIN	"-s"	// -stdin
    else if(!strncmp(arg,OPT_STDIN,strlen(OPT_STDIN) ) ) {
      read_from_stdin = 1;
    }
    else if(!strcmp(arg,"--")) {
      //This causes the following arguments to be positional, even if they
      //begin with "--".  This is the standard getopt convention, even
      //though we are using non-standard long-argument notation for
      //consistency with other Condor tools.
      i++;
      break;
    }
    else {
      IllegalOption(argv[0],arg);
    }
  }


	int num_positional_args = argc - i;
	switch (num_positional_args) {
		case 0:
			if(! read_from_stdin) {
			  stork_print_usage(stderr, argv[0], USAGE, true);
			  exit(1);
			}
		case 1:
			if(read_from_stdin) {
				strcpy(fname, "stdin");
				global_opts.server = argv[i];
			} else {
				strcpy(fname, argv[i]);
			}
		  break;
	  case 2:
			global_opts.server = argv[i++];
			strcpy(fname, argv[i]);
		  break;
	  default:
		  stork_print_usage(stderr, argv[0], USAGE, true);
		  exit(1);
  }

  //open the submit file
  if(read_from_stdin) adfile = stdin;
  else {
    adfile = fopen(fname,"r");
    if (adfile == NULL) {
      fprintf(stderr, "Cannot open submit file %s: %s\n",fname, strerror(errno) );
      exit(1);
    }
  }
  

  int nrequests = 0;


    i =0;
    while (1){
      c = fgetc(adfile);
      if (c == ']'){ 
	leftparan --; 
	if (leftparan == 0) break;
      }
      if (c == '[') leftparan ++; 
      
      if (c == EOF) {
	fprintf (stderr, "Invalid Stork submit file %s\n", fname);
	return 1;
      }
      
      adstr += c;
      i++;
    }
    adstr += c;

    nrequests ++;
    if (nrequests > 1) {
      fprintf (stderr, "Multiple requests currently not supported!\n");
      return 1;
    } 

    //check the validity of the request
    currentAd = parser.ParseClassAd(adstr);
    if (currentAd == NULL) {
      fprintf(stderr, "Invalid input format! Not a valid classad!\n");
      return 1;
    }

    //add lognotes to the submit classad 
    if (lognotes){
      classad::ExprTree *expr = NULL;
      if ( !parser.ParseExpression(lognotes, expr) )
	fprintf(stderr,"Parse error in lognotes\n");
      currentAd->Insert("LogNotes", expr);
    }
    
    //check format of the submit classad
    if ( !check_dap_format(currentAd)){
      fprintf(stderr, "========================\n");
      fprintf(stderr, "Not a valid DaP request!\nPlease check your submit file and then resubmit...\n");
      fprintf(stderr, "========================\n");
      return 1;
    }

    std::string proxy_file_name;
    char * proxy = NULL;
    int proxy_size = 0;
    if (currentAd->EvaluateAttrString ("x509proxy",proxy_file_name )) {

        if ( proxy_file_name == "default" ) {
            char *defproxy = get_x509_proxy_filename();
            if (defproxy) {
                printf("using default proxy: %s\n", defproxy);
                proxy_file_name = defproxy;
                free(defproxy);
            } else {
                fprintf(stderr, "ERROR: %s\n", x509_error_string() );
                return 1;
            }
        }

		struct stat stat_buff;
		if (stat (proxy_file_name.c_str(), &stat_buff) == 0) {
			proxy_size = stat_buff.st_size;
		} else {
                fprintf(stderr, "ERROR: proxy %s: %s\n",
                        proxy_file_name.c_str(),
                        strerror(errno) );
                return 1;
        }

        // Do a quick check on the proxy.
        if ( x509_proxy_try_import( proxy_file_name.c_str() ) != 0 ) {
            fprintf(stderr, "ERROR: check credential %s: %s\n",
                    proxy_file_name.c_str(),
                    x509_error_string() );
            return 1;
        }
        int remaining =
            x509_proxy_seconds_until_expire( proxy_file_name.c_str() );
        if (remaining < 0) {
            fprintf(stderr, "ERROR: check credential %s expiration: %s\n",
                    proxy_file_name.c_str(),
                    x509_error_string() );
            return 1;
        }
        if (remaining == 0) {
            fprintf(stderr, "ERROR: credential %s has expired\n",
                    proxy_file_name.c_str() );
            return 1;
        }

		FILE * fp = fopen (proxy_file_name.c_str(), "r");
		if (fp) {
			proxy = (char*)malloc ((proxy_size+1)*sizeof(char));
            ASSERT(proxy);
			if (fread (proxy, proxy_size, 1, fp) != 1) {
                fprintf(stderr, "ERROR: Unable to read proxy %s: %s\n",
                        proxy_file_name.c_str(),
                        strerror(errno) );
                return 1;
            }
			fclose (fp);
		} else {
			fprintf(stderr, "ERROR: Unable to open proxy %s: %s\n",
                    proxy_file_name.c_str(),
                    strerror(errno) );
			return 1;
        }
    }

    //if input is valid, then send the request:
    classad::PrettyPrint unparser;
	std::string adbuffer = "";
    unparser.Unparse(adbuffer, currentAd);
    fprintf(stdout, "================\n");
    fprintf(stdout, "Sending request:");
    fprintf(stdout, "%s\n", adbuffer.c_str());


	char * job_id = NULL;
	char * error_reason = NULL;
	int rc = stork_submit (currentAd,
						 global_opts.server,
						 proxy, 
						 proxy_size, 
						 job_id,
						 error_reason);
    fprintf(stdout, "================\n");

	if (rc) {
		 fprintf (stdout, "\nRequest assigned id: %s\n", job_id);	
	} else {
		fprintf (stderr, "\nERROR submitting request (%s)!\n", error_reason);
	}


	if (proxy) free(proxy);
	adbuffer = "";

	return (rc)?0:1;
}











