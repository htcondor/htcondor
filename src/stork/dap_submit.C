#include "dap_constants.h"
#include "condor_common.h"
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "internet.h"
#include "condor_config.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

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
void Usage(char *argv0)
{
  fprintf(stderr, "USAGE: %s [options] <host_name|agent> <submit_file>\n"
	"Valid options:\n"
	"\t-help\t\t\tThis screeen\n"
	"\t-debug\t\t\tDisplay debugging info to console\n"
	"\t-lognotes \"notes\"\tadd lognote to submit file before processing\n"
	"\t-stdin\t\t\tread submission from stdin instead of a file\n"
	,argv0);
  exit( 1 );
}

void MissingArgument(char *argv0,char *arg)
{
  fprintf(stderr,"Missing argument: %s\n",arg);
  Usage(argv0);  // this function calls exit()
}

void IllegalOption(char *argv0,char *arg)
{
  fprintf(stderr,"Illegal option: %s\n",arg);
  Usage(argv0);  // this function calls exit()
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
  char fname[MAXSTR], hostname[MAXSTR];
  char *lognotes = NULL;
  int read_from_stdin = 0;

  for(i=1;i<argc;i++) {
    char *arg = argv[i];
    if(arg[0] != '-') {
      break; //this must be a positional argument
    }
    else if(!strcmp(arg,"-lognotes")) {
      if(i+1 >= argc) MissingArgument(argv[0],arg);
      lognotes = argv[++i];
    }
    else if(!strcmp(arg,"-stdin")) {
      read_from_stdin = 1;
    }
    else if(!strcmp(arg,"-debug")) {
      Termlog = 1;
      dprintf_config ("TOOL", 2 );
    }
    else if(!strcmp(arg,"-h")) {
      Usage(argv[0]);  // this function calls exit()
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

  config();

  if(i >= argc) MissingArgument(argv[0],"<host name>");
  strcpy(hostname,argv[i++]);

  
  char * stork_host = get_stork_sinful_string (hostname);
  printf ("Sending request to server %s\n", stork_host);


  if(read_from_stdin) {
    strcpy(fname, "stdin");
  }
  else {
    if(i >= argc) MissingArgument(argv[0],"<submit_file>");
    strcpy(fname, argv[i++]);
  }

  //for backwards compatibility, read a trailing -lognotes option:
  for(;i<argc;i++) {
    if (!strcmp("-lognotes", argv[i])) {
      i++;
      if (argc <= i) {
	fprintf(stderr, "No line to add to the submit file specified!\n" );
	Usage(argv[0]);
      }
      fprintf(stdout, "LogNotes: %s\n",argv[i]);
      
      lognotes = (char*) malloc(MAXSTR * sizeof(char));
      snprintf(lognotes, MAXSTR, "\"%s\"",argv[i]);
    }
    else{
      Usage(argv[0]);
    }
    
  }
  

  //open the submit file
  if(read_from_stdin) adfile = stdin;
  else {
    adfile = fopen(fname,"r");
    if (adfile == NULL) {
      fprintf(stderr, "Cannot open file:%s\n",fname);
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

		struct stat stat_buff;
		if (stat (proxy_file_name.c_str(), &stat_buff) == 0) {
			proxy_size = stat_buff.st_size;
		}

		FILE * fp = fopen (proxy_file_name.c_str(), "r");
		if (fp) {
			proxy = (char*)malloc ((proxy_size+1)*sizeof(char));
			fread (proxy, proxy_size, 1, fp);
			fclose (fp);
		}

		if (proxy == NULL) {
			fprintf(stderr, "ERROR: Unable to read proxy %s\n", proxy_file_name.c_str());
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
						 stork_host, 
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











