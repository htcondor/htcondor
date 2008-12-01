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
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"
#include "condor_daemon_core.h"
#include "daemon.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_config.h"
#include "globus_utils.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

#define USAGE \
"[stork_server] submit_file\n\
stork_server\t\t\tspecify explicit stork server (deprecated)\n\
submit_file\t\t\tstork submit file\n\
\t-lognotes \"notes\"\tadd lognote to submit file before processing\n\
\t-stdin\t\t\tread submission from stdin instead of a file"

struct stork_global_opts global_opts;

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

// Read a file into a std::string.  Return false on error.
static bool
readFile(int fd,char const *filename,std::string *buf)
{
    char chunk[4*1024];

	while(1) {
		ssize_t n = read(fd,chunk,sizeof(chunk)-1);
		if(n>0) {
			chunk[n] = '\0';
			(*buf) += chunk;
		}
		else if(n==0) {
			break;
		}
		else {
			fprintf(stderr,"failed to read %s: %s\n",filename,strerror(errno));
			return false;
		}
	}

	return true;
}

// Skip whitespace in the buffer; returns the offset of the next
// non-whitespace in offset
static int
skip_whitespace(std::string const &s,int &offset) {
	while((int)s.size() > offset && isspace(s[offset])) {
		offset++;
	}
	return offset;
}

// Submit a single ClassAd to the server
int
submit_ad(
	Sock * sock,
	classad::ClassAd *currentAd,
	char *lognotes,
	bool spool_proxy )
{

    //check the validity of the request
    if (currentAd == NULL) {
      fprintf(stderr, "Invalid input format! Not a valid classad!\n");
      return 1;
    }

    //add lognotes to the submit classad 
	// FIXME the lognotes apply to every job, not just a single job.  This will
	// break DAGMan.
    if (lognotes){
        if (! currentAd->InsertAttr("LogNotes", lognotes) ) {
            fprintf(stderr, "error inserting lognotes '%s' into job ad\n",
                    lognotes);
        }
    }
    
    //check format of the submit classad
    if ( !check_dap_format(currentAd)){
      fprintf(stderr, "========================\n");
      fprintf(stderr, "Not a valid DaP request!\nPlease check your submit file and then resubmit...\n");
      fprintf(stderr, "========================\n");
      return 1;
    }

	bool this_job_spool_proxy = spool_proxy;
	if( !currentAd->EvaluateAttrBool("spool_proxy",this_job_spool_proxy) ) {
		this_job_spool_proxy = spool_proxy;
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
				currentAd->InsertAttr("x509proxy", proxy_file_name);
                free(defproxy);
            } else {
                fprintf(stderr, "ERROR: %s\n", x509_error_string() );
                return 1;
            }
        }

		if( this_job_spool_proxy ) {

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
					if (proxy) free(proxy);
					return 1;
				}
				fclose (fp);
			} else {
				fprintf(stderr, "ERROR: Unable to open proxy %s: %s\n",
						proxy_file_name.c_str(),
						strerror(errno) );
				if (proxy) free(proxy);
				return 1;
			}
		}
    }

    //if input is valid, then send the request:
    classad::PrettyPrint unparser;
	std::string adbuffer = "";
    unparser.Unparse(adbuffer, currentAd);
    fprintf(stdout, "================\n");
    fprintf(stdout, "Sending request:");
    fprintf(stdout, "%s\n", adbuffer.c_str());


	char *submit_error_reason = NULL;
	char * job_id = NULL;
	int rc = stork_submit (sock,
						currentAd,
						 global_opts.server,
						 proxy, 
						 proxy_size, 
						 job_id,
						 submit_error_reason);
    fprintf(stdout, "================\n");

	if (rc) {
		 fprintf (stdout, "\nRequest assigned id: %s\n", job_id);	
	} else {
		fprintf (stderr, "\nERROR submitting request (%s)!\n",
				submit_error_reason);
	}
	if (proxy) free(proxy);

	return 0;
}

/* ============================================================================
 * main body of dap_submit
 * ==========================================================================*/
int main(int argc, char **argv)
{
	int i;
	std::string adstr="";
	classad::ClassAdParser parser;
# if 0
	classad::ClassAd *currentAd = NULL;
	int leftparan = 0;
# endif
	std::string fname;
	char *lognotes = NULL;
	int read_from_stdin = 0;

	config();	// read config file

	bool spool_proxy = param_boolean("STORK_SPOOL_PROXY",true);

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
			break;
		case 1:
			if(read_from_stdin) {
				global_opts.server = argv[i];
			} else {
				fname = argv[i];
			}
		  break;
	  case 2:
			global_opts.server = argv[i++];
			fname = argv[i];
		  break;
	  default:
		  stork_print_usage(stderr, argv[0], USAGE, true);
		  exit(1);
	}

	//open the submit file
	int submit_file_fd = -1;
	if(read_from_stdin) {
		submit_file_fd = 0;
		fname = "stdin";
	}
	else {
		submit_file_fd = safe_open_wrapper( fname.c_str(), O_RDONLY, 0 );
		if (submit_file_fd < 0 ) {
			fprintf(stderr, "Cannot open submit file %s: %s\n",
					fname.c_str(), strerror(errno) );
			exit(1);
		}
	}

	std::string adBuf;
	if(!readFile(submit_file_fd, fname.c_str(), &adBuf)) {
		return 1;
	}

	if(!read_from_stdin) {
		close( submit_file_fd );
	}

	// *** NEW ***
	MyString sock_error_reason;

	Sock * sock = start_stork_command_and_authenticate (
		global_opts.server,
		STORK_SUBMIT,
		sock_error_reason);

	const char *host = global_opts.server ? global_opts.server : "unknown";
	if (!sock) {
		fprintf(stderr, "ERROR: connect to server %s: %s\n",
				host, sock_error_reason.Value() );
		return 1;
	}

	// read all classads out of the input file
	int offset = 0;
	int last_offset = 0; //so we can give error message where parser started
	classad::ClassAd ad;

	skip_whitespace(adBuf,offset);
	bool submit_failed = false;
	while (parser.ParseClassAd(adBuf, ad, offset) ) {
		last_offset = offset;

		// TODO: Add transaction processing, so that either all of, or none of
		// the submit ads are added to the job queue.  The current
		// implementation can fail after a partial submit, and not inform the
		// user.

        if (submit_ad(sock, &ad, lognotes, spool_proxy) != 0) {
			submit_failed = true;
			break;
		}
		skip_whitespace(adBuf,offset);
		last_offset = offset;
    }

	offset = last_offset;

	if(!submit_failed) {
		skip_whitespace(adBuf,offset);
		if( (int)adBuf.size() > offset ) {
			fprintf(stderr, "stork_submit: failed to read a ClassAd in the"
					" submit file (%s) beginning with the following text:"
					" %.200s\n",fname.c_str(),adBuf.c_str()+offset);
			submit_failed = true;
		}
	}

	if(submit_failed) {
		return 1;
	}

	//Tell the server we are done sending jobs.
	sock->encode();
	char *goodbye = "";
	sock->code(goodbye);
	sock->eom();

	sock->close();
	delete sock;

	return 0;
}
