/* ============================================================================
 * DaP_Server.C - Data Placement Scheduler Server
 * Reliable File Transfer through SRB-GsiFTP-NeST-SRM.. and other services
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * February 2002 - ...
 * ==========================================================================*/

#include "dap_server.h"
#include "dap_server_interface.h"
#include "dap_classad_reader.h"
#include "dap_logger.h"
#include "dap_utility.h"
#include "dap_error.h"
#include "dap_scheduler.h"
#include "env.h"
#include "daemon.h"
#include "condor_config.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"
#define DAP_CATALOG_NAMESPACE	"stork."

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
 * These are the default values for some global Stork parameters.                        
 * They will be overwritten by the values in the Stork Config file.                   
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
unsigned long Max_num_jobs;            //max number of concurrent jobs running
unsigned long Max_retry;               //max number of times a job will be retried 
unsigned long Max_delayInMinutes;      //max time a job is allowed to finish
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

extern char *logfilename;
extern char *xmllogfilename;
extern char *userlogfilename;
char historyfilename[_POSIX_PATH_MAX];
char *Cred_tmp_dir = NULL;				// temporary credential storage directory
char *Module_dir = NULL;				// Module directory (DAP Catalog)
char *Log_dir = NULL;					// LOG directory

int  daemon_std[3];
int  transfer_dap_reaper_id, reserve_dap_reaper_id, release_dap_reaper_id;
int  requestpath_dap_reaper_id;

unsigned long last_dap = 0;  //changed only on new request

classad::ClassAdCollection      *dapcollection;
Scheduler dap_queue;

int listenfd_submit;
int listenfd_status;
int listenfd_remove;

/* ==========================================================================
 * Open daemon core file pointers.
 * All modules will inherit these standard I/O file descriptors.
 * results in daemon_std[]
 * ==========================================================================*/
void open_daemon_core_file_pointers()
{
	const char* log_dir = param("LOG");
	const char* alt_log_dir = "/tmp";
	const char* name = "/Stork-module.";
	const int flags = O_CREAT | O_RDWR;
	const mode_t mode = 00644;
	MyString dc_stdin, dc_stderr, dc_stdout;

	if (! log_dir ) log_dir = alt_log_dir;

	// Standard input
	dc_stdin = NULL_FILE;
	dprintf(D_ALWAYS, "module standard input redirected from %s\n",
			dc_stdin.Value() );
	daemon_std[0] = open ( dc_stdin.Value(), flags, mode);
	if ( daemon_std[0] < 0 ) {
			dprintf( D_ALWAYS,
				"ERROR: daemoncore stdin fd %d open(%s,%#o,%#o): (%d)%s\n",
					0, dc_stdin.Value(), flags, mode,
					errno, strerror(errno)
			);
	}

	// Standard output
	dc_stdout = log_dir;
	dc_stdout += name;
	dc_stdout += "stdout";
	dprintf(D_ALWAYS, "module standard output redirected to %s\n",
			dc_stdout.Value() );
	daemon_std[1] = open ( dc_stdout.Value(), flags, mode);
	if ( daemon_std[1] < 0 ) {
			dprintf( D_ALWAYS,
				"ERROR: daemoncore stdout fd %d open(%s,%#o,%#o): (%d)%s\n",
					1, dc_stdout.Value(), flags, mode,
					errno, strerror(errno)
			);
	}

	// Standard error
	dc_stderr = log_dir;
	dc_stderr += name;
	dc_stderr += "stderr";
	dprintf(D_ALWAYS, "module standard error redirected to %s\n",
			dc_stderr.Value() );
	daemon_std[2] = open ( dc_stderr.Value(), flags, mode);
	if ( daemon_std[2] < 0 ) {
			dprintf( D_ALWAYS,
				"ERROR: daemoncore stderr fd %d open(%s,%#o,%#o): (%d)%s\n",
					2, dc_stderr.Value(), flags, mode,
					errno, strerror(errno)
			);
	}


	if (log_dir && strcmp(log_dir, alt_log_dir) ) free( (char *)log_dir);
	return;
}

/* ==========================================================================
 * read the config file and set some global parameters
 * ==========================================================================*/
int read_config_file()
{
	//get value for Max_num_jobs
	Max_num_jobs =
		param_integer(
			"STORK_MAX_NUM_JOBS",		// name
			10,							// default
			1							// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAX_NUM_JOBS = %ld\n", Max_num_jobs);  

	//get value for Max_retry
	Max_retry =
		param_integer(
			"STORK_MAX_RETRY",		// name
			10,						// default
			1						// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAX_RETRY = %ld\n", Max_retry);  

	//get value for Max_delayInMinutes
	Max_delayInMinutes =
		param_integer(
			"STORK_MAXDELAY_INMINUTES",	// name
			10,						// default
			1						// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAXDELAY_INMINUTES = %ld\n", Max_delayInMinutes);  

	//get value for STORK_TMP_CRED_DIR
	if (Cred_tmp_dir) free(Cred_tmp_dir);
	Cred_tmp_dir = param("STORK_TMP_CRED_DIR");
	if ( ! Cred_tmp_dir ) Cred_tmp_dir = strdup("/tmp");	// default
	dprintf(D_ALWAYS, "STORK_TMP_CRED_DIR = %s\n", Cred_tmp_dir);

	// get value for STORK_MODULE_DIR
	if (Module_dir) free(Module_dir);
	Module_dir = param("STORK_MODULE_DIR");
	if ( ! Module_dir ) {
		Module_dir = param("LIBEXEC");
		if ( ! Module_dir ) {
			dprintf (D_ALWAYS, "ERROR: STORK_MODULE_DIR not defined in config file!\n");
			DC_Exit (1);
		}
	}
	dprintf(D_ALWAYS, "STORK_MODULE_DIR = %s\n", Module_dir);

	// get value for LOG
	if (Log_dir) free(Log_dir);
	Log_dir = param("LOG");
	if ( ! Log_dir ) Log_dir = strdup("/tmp");	// default
	dprintf(D_ALWAYS, "modules will execute in LOG directory %s\n", Log_dir);
	     
	return TRUE;
}

/* ==========================================================================
 * close daemon core file pointers
 * ==========================================================================*/
void close_daemon_core_file_pointers()
{
	close(daemon_std[0]);
	close(daemon_std[1]);
	close(daemon_std[2]);
}
/* ============================================================================
 * check whether the status of a dap job is already logged or not 
 * ==========================================================================*/
int already_logged(char *dap_id, char *status)
{
	classad::ClassAd       *job_ad;
	char                    current_status[MAXSTR];
	std::string             key;
	char unstripped[MAXSTR];

	key = "key = ";
	key += dap_id;

	job_ad = dapcollection->GetClassAd(key);
	if (!job_ad) {
		return 0; //no such job!
	}
	else{
		getValue(job_ad, "status", unstripped);
		strncpy(current_status, strip_str(unstripped), MAXSTR);

		if (!strcmp(current_status, status)){
			return 1; //already logged
		}
		else {
			return 0; //not logged
		}
	}//else
}

/* ============================================================================
 * get the last dap_id from the dap-jobs-to-process
 * ==========================================================================*/
void get_last_dapid()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	unsigned long           max_dapid = 0;
	char                    dap_id[MAXSTR];
	std::string             key;
	char unstripped[MAXSTR];

	query.Bind(dapcollection);  
	query.Query("root", NULL);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) {
				break; //no matching classad
			}
			else{
				getValue(job_ad, "dap_id", unstripped);
				strncpy(dap_id, strip_str(unstripped), MAXSTR);
	
				if ((unsigned long)atol(dap_id) > max_dapid) {
					max_dapid = atol(dap_id);
				}
			}
		}while (query.Next(key));
	}
  
		//check also the history log file for max dapid
	ClassAd_Reader adreader(historyfilename);
	while(1) {
		if ( !adreader.readAd()) {
			break;
		}
		adreader.getValue("dap_id", unstripped);
		strncpy(dap_id, strip_str(unstripped), MAXSTR);

		if ((unsigned long)atol(dap_id) > max_dapid) {
			max_dapid = atol(dap_id);
		}
	}
		//end of while
  
	last_dap = max_dapid;
}

/* ============================================================================
 * dap transfer function
 * ==========================================================================*/
int transfer_dap(char *dap_id, char *src_url, char *dest_url, char *arguments, char * cred_file_name)
{

	char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	char command[MAXSTR], commandbody[MAXSTR], argument_str[MAXSTR];
	int pid;
	char unstripped[MAXSTR];

	parse_url(src_url, src_protocol, src_host, src_file);
	parse_url(dest_url, dest_protocol, dest_host, dest_file);

	strcpy(unstripped, src_url);
	strncpy(src_url, strip_str(unstripped), MAXSTR);  

	strcpy(unstripped, dest_url);
	strncpy(dest_url, strip_str(unstripped), MAXSTR);  

	strcpy(unstripped, arguments);
	strncpy(arguments, strip_str(unstripped), MAXSTR);  

	snprintf(commandbody, MAXSTR, "%stransfer.%s-%s",
		DAP_CATALOG_NAMESPACE, src_protocol, dest_protocol);

		//create a new process to transfer the files
	snprintf(command, MAXSTR, "%s/%s", Module_dir, commandbody);
	snprintf(argument_str ,MAXSTR, "%s %s %s %s", 
			 commandbody, src_url, dest_url, arguments);
  

		// If using GSI proxy set up the environment to point to it
	Env myEnv;
	if (!cred_file_name) {
		cred_file_name = "";
	}

	dprintf (D_FULLDEBUG, "Using user credential %s\n", cred_file_name);
	MyString newenv_buff;

	// child process will need additional environments
	newenv_buff="X509_USER_PROXY=";
	newenv_buff+=cred_file_name;
	myEnv.Put (newenv_buff.Value());
    
	newenv_buff="X509_USER_KEY=";
	newenv_buff+=cred_file_name;
	myEnv.Put (newenv_buff.Value());

	newenv_buff="X509_USER_CERT=";
	newenv_buff+=cred_file_name;
	myEnv.Put (newenv_buff.Value());

	char *env_string = myEnv.getDelimitedString();	// return string from "new"

	// Create child process via daemoncore
	MyString src_url_value, dest_url_value;
	src_url_value.sprintf("\"%s\"", src_url);
	dest_url_value.sprintf("\"%s\"", dest_url);
	write_xml_user_log(userlogfilename, "MyType", "\"GenericEvent\"", 
					   "EventTypeNumber", "8", 
					   "Cluster", dap_id,
					   "Proc", "-1",
					   "Subproc", "-1",
					   "Type", "transfer",
					   "SrcUrl", (char *)src_url_value.Value(),
					   "DestUrl", (char *)dest_url_value.Value(),
					   "Arguments", arguments,
					   "CredFile", cred_file_name);

	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 argument_str,					// args string
		 PRIV_USER_FINAL,				// privilege state
		 transfer_dap_reaper_id,		// reaper id
		 FALSE,							// do not want a command port
		 env_string,                	// colon seperated environment string
		 Log_dir,						// current working directory
		 FALSE,							// do not create a new process group
		 NULL,							// list of socks to inherit
		 daemon_std						// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	if (env_string) delete []env_string;// delete string from "new"
	if (pid > 0) {
		dap_queue.insert(dap_id, pid);
		return DAP_SUCCESS;
	}
	else{
		transfer_dap_reaper(NULL, 0 ,111); //executable not found!
		return DAP_ERROR;                  //--> Find a better soln!
	}
  
}

/* ============================================================================
 * dap reserve function
 * ==========================================================================*/
void reserve_dap(char *dap_id, char *reserve_id, char *reserve_size, char *duration, char *dest_url, char *output_file)
{

	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	char command[MAXSTR], commandbody[MAXSTR], argument_str[MAXSTR];
	int pid;
  
	parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%sreserve.%s",
		DAP_CATALOG_NAMESPACE, dest_protocol);

		//create a new process to transfer the files
	snprintf(command, MAXSTR, "%s/%s", Module_dir, commandbody);
	snprintf(argument_str ,MAXSTR, "%s %s %s %s %s", 
			 commandbody, dest_host, output_file, reserve_size, duration);

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 argument_str,					// args string
		 PRIV_USER_FINAL,				// privilege state
		 reserve_dap_reaper_id,			// reaper id
		 FALSE,							// do not want a command port
		 NULL,  		              	// colon seperated environment string
		 Log_dir,						// current working directory
		 FALSE,							// do not create a new process group
		 NULL,							// list of socks to inherit
		 daemon_std						// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	dap_queue.insert(dap_id, pid);
}

/* ============================================================================
 * dap release function
 * ==========================================================================*/
void release_dap(char *dap_id, char *reserve_id, char *dest_url)
{
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	char command[MAXSTR], commandbody[MAXSTR], argument_str[MAXSTR];
	int pid;
  
	parse_url(dest_url, dest_protocol, dest_host, dest_file);

		//get the corresponding lot_id
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	char                    lot_id[MAXSTR] = "", current_reserve_id[MAXSTR];
	std::string             key, constraint;
	classad::ClassAdParser  parser;
	char unstripped[MAXSTR];

	constraint = "other.dap_type == \"reserve\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS, 
				"Error in parsing constraint: %s\n", constraint.c_str());
	}

	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) {
				dprintf(D_ALWAYS,"No matching add!\n");
				return; //no matching classad
			}
			else{
				getValue(job_ad, "reserve_id", unstripped);
				strncpy(current_reserve_id, strip_str(unstripped), MAXSTR);
	
				if (!strcmp(current_reserve_id, reserve_id)) {
					getValue(job_ad, "lot_id", unstripped);
					strncpy(lot_id, strip_str(unstripped), MAXSTR);
				}
			}
		}while (query.Next(key));
	}

		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%srelease.%s",
		DAP_CATALOG_NAMESPACE, dest_protocol);
  
		//dynamically create a shell script to execute
	snprintf(command, MAXSTR, "%s/%s",  Module_dir, commandbody);
  
		//create a new process to transfer the files
	snprintf(argument_str ,MAXSTR, "%s %s %s", 
			 commandbody, dest_host, lot_id);

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 argument_str,					// args string
		 PRIV_USER_FINAL,				// privilege state
		 release_dap_reaper_id,			// reaper id
		 FALSE,							// do not want a command port
		 NULL, 			               	// colon seperated environment string
		 Log_dir,						// current working directory
		 FALSE,							// do not create a new process group
		 NULL,							// list of socks to inherit
		 daemon_std						// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	dap_queue.insert(dap_id, pid);
	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * dap reserve function
 * ==========================================================================*/
void requestpath_dap(char *dap_id, char *src_url, char *dest_url)
{

	char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];

	char command[MAXSTR], commandbody[MAXSTR], argument_str[MAXSTR];
	int pid;
  
	parse_url(src_url, src_protocol, src_host, src_file);
	parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%srequestpath.%s",
		DAP_CATALOG_NAMESPACE, src_protocol);

		//create a new process to transfer the files
	snprintf(command, MAXSTR, "%s/%s", Module_dir, commandbody);
	snprintf(argument_str ,MAXSTR, "%s %s %s", 
			 commandbody, src_host, dest_host);

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 argument_str,					// args string
		 PRIV_USER_FINAL,				// privilege state
		 requestpath_dap_reaper_id,		// reaper id
		 FALSE,							// do not want a command port
		 NULL,  		              	// colon seperated environment string
		 Log_dir,						// current working directory
		 FALSE,							// do not create a new process group
		 NULL,							// list of socks to inherit
		 daemon_std						// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	dap_queue.insert(dap_id, pid);
}

/* ============================================================================
 * process the request last read..
 * - if the request is already logged as being in progress, don't log it again
 * ==========================================================================*/
void process_request(classad::ClassAd *currentAd)
{
	char dap_id[MAXSTR], dap_type[MAXSTR];
	char unstripped[MAXSTR];
  
		//get the dap_id of the request
	getValue(currentAd, "dap_id", unstripped); 
	strncpy(dap_id, strip_str(unstripped), MAXSTR);

		//log the new status of the request
	write_collection_log(dapcollection, dap_id, 
						 "status = \"processing_request\"");
  
	char lognotes[MAXSTR];
	getValue(currentAd, "LogNotes", lognotes);
  
	write_xml_user_log(userlogfilename, "MyType", "\"ExecuteEvent\"", 
					   "EventTypeNumber", "1", 
					   "Cluster", dap_id,
					   "Proc", "-1",
					   "Subproc", "-1",
					   "LogNotes", lognotes);

  
		//write_xml_log(xmllogfilename, currentAd, "\"processing_request\"");
  

		//get the dap_type
	getValue(currentAd, "dap_type", unstripped); 
	strncpy(dap_type, strip_str(unstripped), MAXSTR);


		// Init user id for the right user
	std::string key="owner";
	std::string _owner;
	const char * owner = NULL;

	if (currentAd->EvaluateAttrString(key, _owner)) {
		owner = _owner.c_str();
	}

	if (!init_user_id_from_FQN (owner) ) {
		dprintf (D_ALWAYS, "Unable to find local user for \"%s\"\n", owner);
	}


		// Check for GSI proxy
	char * cred_file_name = NULL;
	key="cred_name";
	std::string cred_name;
	if (currentAd->EvaluateAttrString(key, cred_name)) {

			// If "cred_name" attribute is present, ask credd for the
			// credential We should do this each time, since credd may refresh
			// credentials, etc

		void * buff = NULL;
		int size = 0;
		if (get_cred_from_credd (cred_name.c_str(), buff, size)) {
			cred_file_name = get_credential_filename (dap_id);

				// Save to file

				// Create file as root
			priv_state old_priv = set_root_priv();
			int fd = open (cred_file_name, O_WRONLY | O_CREAT );
			if (fd > -1) {
				if ( fchmod (fd, S_IRUSR | S_IWUSR) < 0 ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred file fchmod(%d,%#o): (%d)%s\n",
							__FILE__, __LINE__,
							fd, S_IRUSR | S_IWUSR,
							errno, strerror(errno)
					);
				}
				if ( fchown (fd, get_user_uid(), get_user_gid() ) < 0 ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred file fchown(%d,%d,%d): (%d)%s\n",
							__FILE__, __LINE__,
							fd,  get_user_uid(), get_user_gid(),
							errno, strerror(errno)
					);
				}
			}
			set_priv(old_priv);

			if (fd > -1) {
				int nbytes = write (fd, buff, size);
				if ( nbytes < size ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred short write: %d out of %d (%d)%s\n",
							__FILE__, __LINE__,
							nbytes, size,
							errno, strerror(errno)
					);
				}
				close (fd);
			}
			else {
				dprintf(
					D_ALWAYS,
					"Unable to create credential storage file %s !\n%d(%s)\n",
					cred_file_name,
					errno,
					strerror(errno)
				);
			}
			free (buff);
		} else {
			dprintf (D_ALWAYS, "Unable to receive credential %s from CREDD\n", cred_name.c_str());
		}
	}
 
		// If proxy was submitted directly to stork, use it
	if (cred_file_name == NULL) {
		std::string cred_file_name_str;
		if (currentAd->EvaluateAttrString("x509proxy", cred_file_name_str))
			cred_file_name=strdup(cred_file_name_str.c_str());
	}

		//if dap_type == transfer
	if (!strcmp(dap_type, "transfer")){
    
		char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
		char use_protocol[MAXSTR], alt_protocols[MAXSTR];
		char src_alt_protocol[MAXSTR], dest_alt_protocol[MAXSTR];
		char next_protocol[MAXSTR];
		int protocol_num;

		getValue(currentAd, "src_url", src_url);
		getValue(currentAd, "dest_url", dest_url);
		getValue(currentAd, "arguments", arguments);

			//select which protocol to use
		getValue(currentAd, "use_protocol", use_protocol);
		protocol_num = atoi(use_protocol);
    
		if (!strcmp(use_protocol, "0")){
				//dprintf(D_ALWAYS, "use_protocol: %s\n", use_protocol);      
			transfer_dap(dap_id, src_url, dest_url, arguments, cred_file_name);
		}
		else{
			getValue(currentAd, "alt_protocols", alt_protocols);
			dprintf(D_ALWAYS, "alt. protocols = %s\n", alt_protocols);
      
			if (!strcmp(alt_protocols,"")) { //if no alt. protocol defined
				transfer_dap(dap_id, src_url, dest_url, arguments, cred_file_name);
			}
			else{ //use alt. protocols
				strcpy(next_protocol, strtok(alt_protocols, ",") );   
	
				for(int i=1;i<protocol_num;i++){
					strcpy(next_protocol, strtok(NULL, ",") );   
				}

				dprintf(D_ALWAYS, "next protocol = %s\n", next_protocol);
	
				if (strcmp(next_protocol,"")) {
					strcpy(src_alt_protocol, strtok(next_protocol, "-") );   
					strcpy(dest_alt_protocol, strtok(NULL, "") );   
	  
						//printf("src protocol = %s\n", strip_str(src_alt_protocol));
						//printf("dest protocol = %s\n", strip_str(dest_alt_protocol));

				}
	
				char src_alt_url[MAXSTR], dest_alt_url[MAXSTR];
				char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
				char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	
				parse_url(src_url, src_protocol, src_host, src_file);
				parse_url(dest_url, dest_protocol, dest_host, dest_file);
	
				if (!strcmp(strip_str(src_alt_protocol), "file"))
					snprintf(src_alt_url, MAXSTR, "file:%s", src_file);  
				else
					snprintf(src_alt_url, MAXSTR, "%s://%s/%s", 
							 strip_str(src_alt_protocol), src_host, src_file);
	
				if (!strcmp(strip_str(dest_alt_protocol), "file"))
					snprintf(dest_alt_url, MAXSTR, "file:%s", dest_file);  
				else
					snprintf(dest_alt_url, MAXSTR, "%s://%s/%s", 
							 strip_str(dest_alt_protocol), dest_host, dest_file);
	
	
					//--> These "arguments" may need to be removed or chaged !!
				transfer_dap(dap_id, src_alt_url, dest_alt_url, arguments, cred_file_name);
			}// end use alt. protocols
		}
	}

		//if dap_type == reserve 
	else if(!strcmp(dap_type, "reserve")){

		char dest_host[MAXSTR], reserve_size[MAXSTR];
		char duration[MAXSTR], reserve_id[MAXSTR];
		char output_file[MAXSTR];

		getValue(currentAd, "dest_host", dest_host);
		strncpy(dest_host, strip_str(dest_host), MAXSTR);
    
		getValue(currentAd, "output_file", output_file); 
		strncpy(output_file, strip_str(output_file), MAXSTR);

		getValue(currentAd, "reserve_size", reserve_size); 
		strncpy(reserve_size, strip_str(reserve_size), MAXSTR);

		getValue(currentAd, "duration", duration); 
		strncpy(duration, strip_str(duration), MAXSTR);

		getValue(currentAd, "reserve_id", reserve_id); 
		strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

		reserve_dap(dap_id, reserve_id, reserve_size, duration, dest_host, output_file);
	}

		//if dap_type == release
	else if(!strcmp(dap_type, "release")){
  
		char dest_host[MAXSTR], reserve_id[MAXSTR];

		getValue(currentAd, "dest_host", dest_host); 
		strncpy(dest_host, strip_str(dest_host), MAXSTR);
    
		getValue(currentAd, "reserve_id", reserve_id); 
		strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

		release_dap(dap_id, reserve_id, dest_host);
	}
  
		//if dap_type == requestpath
	else if(!strcmp(dap_type, "requestpath")){
  
		char src_host[MAXSTR], dest_host[MAXSTR];

		getValue(currentAd, "src_host", src_host); 
		strncpy(src_host, strip_str(src_host), MAXSTR);

		getValue(currentAd, "dest_host", dest_host); 
		strncpy(dest_host, strip_str(dest_host), MAXSTR);

			//getValue(currentAd, "path_id", path_id); 
			//strncpy(path_id, strip_str(path_id), MAXSTR);
    
		requestpath_dap(dap_id, src_host, dest_host);
	}

	if (cred_file_name)
		free (cred_file_name);

}

/* ============================================================================
 * regularly check for requests which are in the state of being processed but 
 * not completed yet
 * ==========================================================================*/
void regular_check_for_requests_in_process()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
	char   dap_id[MAXSTR], timestamp[MAXSTR];
	double timediff_inseconds, maxdelay_inseconds;
	int num_attempts = 0; 
		
		//  printf("regular check for requests in process..\n");
  
	maxdelay_inseconds = Max_delayInMinutes * 60;
  
		//set the constraint for the query
	constraint = "other.status == \"processing_request\"";
		//constraint = "other.status == \"request_completed\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS, 
				"Error in parsing constraint: %s\n", constraint.c_str());
	}

	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
					//create a new classad for time comparison
				classad::ClassAd *classad = new classad::ClassAd;
				classad::ClassAdParser parser;
				classad::ExprTree *expr = NULL;

				classad->Insert("currenttime",classad::Literal::MakeAbsTime());

				getValue(job_ad, "timestamp", timestamp); 
				if ( !parser.ParseExpression(timestamp, expr) ) {
					dprintf(D_ALWAYS,"Parse error\n");
				}
				classad->Insert("timeofrequest", expr);

				if( !parser.ParseExpression("InSeconds(currenttime - timeofrequest)", 
											expr) ) {
					dprintf(D_ALWAYS,"Parse error\n");
				}
				classad->Insert("diffseconds", expr);

				if (!classad->EvaluateAttrNumber("diffseconds", timediff_inseconds)) {
					dprintf(D_ALWAYS, "Error in parsing the attribute: diffseconds\n");
				}
	
					//if the elapsed time is greater than the maximum delay allowed,
					//then reprocess the request..

				if (timediff_inseconds > maxdelay_inseconds){
					getValue(job_ad, "dap_id", dap_id); 
					dprintf(D_ALWAYS, 
							"dap_id: %s, not completed in %d minutes. Reprocessing...\n", 
							dap_id, (int)timediff_inseconds/60);
	  
	  
	  
					unsigned int pid;
					if (dap_queue.get_pid(pid, dap_id) == DAP_SUCCESS){

						if (dap_queue.remove(dap_id) == DAP_SUCCESS){
							dprintf(D_ALWAYS, "Killing process %d, and removing dap %s\n", 
									pid, dap_id);
							if ( kill(pid, SIGKILL) < 0 ) {
								dprintf( D_ALWAYS,
										"%s:%d:kill pid %d error: (%d)%s\n",
										__FILE__, __LINE__,
										pid,
										errno, strerror(errno)
								);
							}

							char attempts[MAXSTR], tempstr[MAXSTR];
							std::string modify_s = "";

								//get num_attempts
							if (getValue(job_ad, "num_attempts", attempts) != DAP_SUCCESS)
								num_attempts = 0;
							else
								num_attempts = atoi(attempts);
		
							dprintf(D_ALWAYS, "dap id: %s, attempt # %d\n", dap_id, num_attempts+2);
		
								//		string sstatus = "";
		
							if (num_attempts + 1 < (int)Max_retry){
									//		  sstatus = "\"request_rescheduled\"";
								modify_s += "status = \"request_rescheduled\";";
							}
		 
							else{
									//		  sstatus = "\"request_failed\"";
								modify_s += "status = \"request_failed\";";
							}
		 
							snprintf(tempstr, MAXSTR, 
									 "num_attempts = %d;",num_attempts + 1);
		 
							modify_s +=  tempstr;
							modify_s += "error_code = \"\"";
		 
							write_collection_log(dapcollection, dap_id, modify_s.c_str());
		 
								//when request is failed for good, remove it from the collection 
								//and log it to the history log file..



							char lognotes[MAXSTR] ;
							getValue(job_ad, "LogNotes", lognotes);
		   
							if (num_attempts + 1 >=  (int)Max_retry){

								remove_credential (dap_id);


								dapcollection->RemoveClassAd(key);
								write_dap_log(historyfilename, "request_failed", 
											  "dap_id", dap_id, "error_code", "\"ABORTED!\"");
		   
								char exit_status_str[MAXSTR];
								snprintf(exit_status_str, MAXSTR, "%d", 9);
		   
								write_xml_user_log(userlogfilename, "MyType", "\"JobCompletedEvent\"", 
												   "EventTypeNumber", "5", 
												   "TerminatedNormally", "1",
												   "ReturnValue", exit_status_str,
												   "Cluster", dap_id,
												   "Proc", "-1",
												   "Subproc", "-1",
												   "LogNotes", lognotes);
		   
								dprintf(D_ALWAYS, "Request aborted!\n");		   
								continue;
							}
		 
						}
						else{
							dprintf(D_ALWAYS, "Error in Removing process %d\n", pid);
						}
	      
					}
	    
					char use_protocol[MAXSTR];

					getValue(job_ad, "use_protocol", use_protocol);

					if (dap_queue.get_numjobs() < Max_num_jobs){
		
						if (num_attempts + 2 < (int)Max_retry){
							process_request(job_ad);
						}
		
					}
	    
				}
	
				if (classad != NULL) delete classad;	
			}//else
		}while (query.Next(key));
	}//if
	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * check at the startup for requests which are in the state of being processed  * but not completed yet
 * ==========================================================================*/
void startup_check_for_requests_in_process()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
  
		//set the constraint for the query
	constraint = "other.status == \"processing_request\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,"Error in parsing constraint!\n");
	}

	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
				if (dap_queue.get_numjobs() < Max_num_jobs)
					process_request(job_ad);
			}
		}while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;
}
/* ============================================================================
 * check for requests which are rescheduled for execution
 * ==========================================================================*/
void regular_check_for_rescheduled_requests()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
  
		//set the constraint for the query
	constraint = "other.status == \"request_rescheduled\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,"Error in parsing constraint!\n");
	}

	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
				if (dap_queue.get_numjobs() < Max_num_jobs)
					process_request(job_ad);
			}
		}while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * initialize dapcollection 
 * ==========================================================================*/
void initialize_dapcollection()
{
	dapcollection = new classad::ClassAdCollection();

	if (dapcollection == NULL ){
		dprintf(D_ALWAYS,"Error: Failed to construct dap collection!\n");
	}

	if (!dapcollection->InitializeFromLog(logfilename)) {
		dprintf(D_ALWAYS,"Couldn't recover ClassAdCollection from log: %s!\n",
				logfilename);
	}
}


/* ============================================================================
 * make some initializations before calling main()
 * ==========================================================================*/
int initializations()
{

		//initialize and open daemoncore file pointers
	open_daemon_core_file_pointers();


		//read the config file and set values for some global parameters
	if (!read_config_file()) {
		return FALSE;
	}

	// Module_dir must be a valid directory path.
	struct stat stat_buff;
	if (stat (Module_dir, &stat_buff) != 0) {
		dprintf (D_ALWAYS, "Invalid value for STORK_MODULE_DIR %s: %s\n",
				Module_dir,
				strerror(errno)
				);
		return FALSE;
	}
	if ( ! S_ISDIR(stat_buff.st_mode) ) {
		dprintf (D_ALWAYS, "STORK_MODULE_DIR %s is not a directory\n",
				Module_dir
				);
		return FALSE;
	}

		//initialize dapcollection 
	initialize_dapcollection();

		//init history file name 
	snprintf(historyfilename, MAXSTR, "%s.history", logfilename);

		//init xml log file name 
		//  sprintf(xmllogfilename, "%s.xml", logfilename);<-- change this

	if (!userlogfilename){
		userlogfilename = (char*) malloc(MAXSTR * sizeof(char));
		if ( userlogfilename ) {
			snprintf(userlogfilename, MAXSTR, "%s.user_log", logfilename);
		} else {
			dprintf( D_ALWAYS,
					"%s:%d: malloc userlogfilename %s.user_log: (%d)%s\n",
					__FILE__, __LINE__,
					logfilename,
					errno, strerror(errno)
			);
		}
	}
  
		//get the last dap_id from dap-jobs-to-process
	get_last_dapid();  

		//check the logfile for noncompleted requests at startup
	startup_check_for_requests_in_process();

	return TRUE;
}

/* ============================================================================
 * main body of the condor_srb_reqex
 * ==========================================================================*/
int call_main()
{

	classad::ClassAd       *job_ad;
	classad::LocalCollectionQuery query;
	classad::ClassAdParser       parser;
	std::string                  key, constraint;
  
		//setup constraints for the query
	constraint = "other.status == \"request_received\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );

	if (!constraint_tree) {
		dprintf(D_ALWAYS, "Error in parsing constraint!\n");
	}

  
	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
				if (dap_queue.get_numjobs() < Max_num_jobs)
					process_request(job_ad);
			}
		}while (query.Next(key));
	}

	if (constraint_tree != NULL) delete constraint_tree;

	return TRUE;
}


/* ============================================================================
 * write incoming dap requests to file: <dap-jobs-to-process>
 * ==========================================================================*/
int write_requests_to_file(ReliSock * sock)
{
	char line[MAX_TCP_BUF];

		// Require authentication 
    if (!sock->isAuthenticated()) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! sock->authenticate(methods.Value(), &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
    }



    sock->decode();

    char * pline = (char*)line;
    if (!sock->code(pline)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error (request)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

    int cred_size;
    

    if (!sock->code(cred_size)) {
		dprintf(D_ALWAYS, "%s:%d Server: recv error (cred size)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

   
    void*cred_buff = NULL;
    if (cred_size > 0) {
		cred_buff = malloc (cred_size);
		if (!sock->code_bytes(cred_buff, cred_size)) {
			dprintf(D_ALWAYS, "%s:%d Server: recv error (cred)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
			return FALSE;
		}
    }
    
		//convert request into a classad
    classad::ClassAdParser parser;
    classad::ClassAd *requestAd = NULL;
    classad::PrettyPrint unparser;
	std::string adbuffer = "";
    char last_dapstr[MAXSTR];
    classad::ExprTree *expr = NULL;

	std::string linestr = line;
		//check the validity of the request
    requestAd = parser.ParseClassAd(linestr);
    if (requestAd == NULL) {
		dprintf(D_ALWAYS, "Invalid input format! Not a valid classad!\n");
		return FALSE;
    }

		// Insert "owner" attribute
    char owner[MAXSTR];
    sprintf (owner, "\"%s\"", sock->getFullyQualifiedUser());
    if ( !parser.ParseExpression (owner, expr) ) {
		dprintf(D_ALWAYS,"Parse error (owner)\n");
		return FALSE;
    }else {
		requestAd->Delete("owner");
		requestAd->Insert("owner", expr);
    }


		//add the dap_id to the request
    snprintf(last_dapstr, MAXSTR, "%ld", last_dap + 1);
    if ( !parser.ParseExpression(last_dapstr, expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("dap_id", expr);

		//add the status to the request
    if ( !parser.ParseExpression("\"request_received\"", expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("status", expr);
    requestAd->Insert("timestamp",classad::Literal::MakeAbsTime());


		//use the default protocol first
    if ( !parser.ParseExpression("0", expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("use_protocol", expr);

    if (cred_buff) {
		char _dap_id_buff[10];
		sprintf (_dap_id_buff, "%ld", last_dap+1);
		char * cred_file_name = get_credential_filename (_dap_id_buff);

		if (!init_user_id_from_FQN(sock->getFullyQualifiedUser())) {
			dprintf (D_ALWAYS, "ERROR: Unable to find local user for \"%s\"\n", sock->getFullyQualifiedUser());
		}
     
			// Switch to root
		priv_state old_priv = set_root_priv();
		int fd = open (cred_file_name, O_WRONLY | O_CREAT);
		if (fd > -1) {
			if ( fchmod (fd, S_IRUSR | S_IWUSR) < 0 ) {
				dprintf( D_ALWAYS,
						"%s:%d: cred file fchmod(%d,%#o): (%d)%s\n",
						__FILE__, __LINE__,
						fd, S_IRUSR | S_IWUSR,
						errno, strerror(errno)
				);
			}
			if ( fchown (fd, get_user_uid(), get_user_gid() ) < 0 ) {
				dprintf( D_ALWAYS,
						"%s:%d: cred file fchown(%d,%d,%d): (%d)%s\n",
						__FILE__, __LINE__,
						fd,  get_user_uid(), get_user_gid(),
						errno, strerror(errno)
				);
			}
		}
			// Switch back to non-priviledged user
		set_priv(old_priv);

		if (fd > -1) {
			int nbytes = write (fd, cred_buff, cred_size);
			if ( nbytes < cred_size ) {
				dprintf( D_ALWAYS,
						"%s:%d: cred short write: %d out of %d (%d)%s\n",
						__FILE__, __LINE__,
						nbytes, cred_size,
						errno, strerror(errno)
				);
			}
			close (fd);
		} else {
			dprintf(
				D_ALWAYS,
				"Unable to create credential storage file %s !\n%d(%s)\n",
				cred_file_name,
				errno,
				strerror(errno)
			);
			return FALSE;
		}

		free (cred_buff);
      
		char cred_file_name_expr[MAXSTR];
		sprintf (cred_file_name_expr, "\"%s\"", cred_file_name);
		if ( !parser.ParseExpression(cred_file_name_expr, expr) ) {
			dprintf(D_ALWAYS,"Parse error (cred file name)\n");
		}

		requestAd->Insert("x509proxy", expr);
		free (cred_file_name);
    } // if cred_buff

		//insert the request to the dap collection
    char keystr[MAXSTR];
    snprintf(keystr, MAXSTR, "key = %ld", last_dap + 1);

		//add the rquest classad to dap collection
    dapcollection->AddClassAd(keystr, requestAd);
    
		//increment last dap_id
    last_dap ++;

    unparser.Unparse(adbuffer, requestAd);
    dprintf(D_ALWAYS, "New Request => %s\n",adbuffer.c_str());

    
		//---> XML Stuff
		//ClassAdXMLUnParser  xmlunparser;
		//string xmlstring;
		//xmlunparser.SetCompactSpacing(false);
		//xmlunparser.Unparse(xmlstring, requestAd);
		//printf("new Request in XML format=> %s\n",xmlstring.c_str());
		//write_xml_log(xmllogfilename, requestAd, "\"request_received\"");
    
    
		//write XML user logs
    char lognotes[MAXSTR];
    getValue(requestAd, "LogNotes", lognotes);
    
    write_xml_user_log(userlogfilename, "MyType", "\"SubmitEvent\"", 
					   "EventTypeNumber", "0", 
					   "Cluster", last_dapstr,
					   "Proc", "-1",
					   "Subproc", "-1",
					   "LogNotes", lognotes);
    



		//send response to the client
    char response[MAXSTR];
    
    snprintf(response, MAXSTR, "%s", last_dapstr);

    sock->encode();
    pline = (char*)response;
    if ( !sock->code (pline)){
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }
    

    return TRUE;
}


int list_queue(ReliSock * sock)
{
	std::string adbuffer = "";
	classad::ClassAdUnParser unparser;


		// Require authentication
	if (!sock->isAuthenticated()) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! sock->authenticate(methods.Value(), &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}

		// Determine the onwer of the request
	const char * request_owner = sock->getFullyQualifiedUser();

		// Create a query
	std::string constraint;
	constraint = "other.owner == ";
	constraint += "\"";
	constraint += request_owner;
	constraint += "\"";

	classad::ClassAdParser parser;
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS, 
				"Error in parsing constraint: %s\n", constraint.c_str());
		return FALSE;
	}

	classad::LocalCollectionQuery query;
	query.Bind(dapcollection);  
	query.Query("root", constraint_tree);
	query.ToFirst();

		// Get the list of results
	StringList result_list;
	std::string key;
	if (query.Current (key)) {
		do {
			classad::ClassAd * job_ad = dapcollection->GetClassAd (key);
			if (job_ad) {
				std::string adbuffer = "";
				unparser.Unparse(adbuffer, job_ad);
				result_list.append (adbuffer.c_str());
			}
		} while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;

	int result_size = result_list.number();


		// Send back the results
	sock->encode();
	
	if (!(sock->code(result_size))) {
		return FALSE;
	}

	result_list.rewind();
	char * job_ad_str = NULL;

	while ((job_ad_str = result_list.next()) != NULL) {

		if (!sock->code (job_ad_str)) {
			return FALSE;
		}
	}

	sock->eom();
	return TRUE;
}

int send_dap_status_to_client(ReliSock * sock)
{
	char nextdap[MAXSTR];



		// Require authentication
	if (!sock->isAuthenticated()) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! sock->authenticate(methods.Value(), &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}


    sock->decode();

    char * dap_id = NULL;
    if (!sock->code (dap_id)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

    const classad::ClassAd                *job_ad;


	bool found_job = false;
	std::string adbuffer;
	classad::ClassAdUnParser unparser;

		// Try getting the job from the queue
	std::string key;
    key = "key = ";
    key += dap_id;
    job_ad = dapcollection->GetClassAd(key);


    if (job_ad) {
		found_job = true;
		unparser.Unparse (adbuffer, job_ad);
	} else {
		// If not, try getting it from the history file
		ClassAd_Reader adreader(historyfilename);
      
		while(adreader.readAd()) {
			adreader.getValue("dap_id", nextdap); 
			strncpy(nextdap, strip_str(nextdap), MAXSTR);
      
			if ( !strcmp(dap_id, nextdap) ){
				found_job = true;
				unparser.Unparse (adbuffer, adreader.getCurrentAd());
				break;
			}
		}
	}
    
	int rc = 1;


		//if the history for dap_id still not found:
    if (!found_job) {
		rc = 0;
		dprintf(D_ALWAYS, 
				"Unable to get status for job with job id: %s\n", 
				dap_id);
		adbuffer = "Couldn't find DaP job: ";
		adbuffer += dap_id;
    } else {
		rc = 1;
		dprintf(D_FULLDEBUG, 
				"status report for the job with job id: %s ==>\n%s\n",
				dap_id, 
				adbuffer.c_str());    
	}
	if (dap_id) free (dap_id);
    
    sock->encode();

    char * pstr = strdup(adbuffer.c_str());
	if (pstr == NULL) {
		EXCEPT("Out of memory. Aborting.");
	}
    if (!(sock->code(rc) && sock->code (pstr))) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		if (pstr) free (pstr);
		return FALSE;
    }
    sock->eom();
	if (pstr) free (pstr);
    return TRUE;
}

/* ============================================================================
 * remove requests from queue (via DaemonCore)
 * ==========================================================================*/
int remove_requests_from_queue(ReliSock * sock)
{
	std::string adbuffer = "";
	classad::ClassAdUnParser unparser;
	int rc = 0;
	
		// Require authentication 
	if (!sock->isAuthenticated()) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! sock->authenticate(methods.Value(), &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}

  
    char * dap_id = NULL;

    sock->decode();

    if (!sock->code (dap_id)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

    classad::ClassAd            *job_ad;
	std::string                  key;

		//search the collection for the classad
    key = "key = ";
    key += dap_id;

    job_ad = dapcollection->GetClassAd(key);
    if (job_ad) {

		unsigned int pid;

		if (dap_queue.get_pid(pid, dap_id) == DAP_SUCCESS){
			dprintf(D_ALWAYS, "Killing process: %d for job: %s\n", pid, dap_id);
			kill(pid, SIGKILL);
		}
		else{
			dprintf(D_ALWAYS, "Corresponding process for job: %s not found\n", dap_id);
		}
      
      
		if (dap_queue.remove(dap_id) == DAP_SUCCESS){
			dprintf(D_ALWAYS, "Removed job: %s\n", dap_id);
		}
		else{
			dprintf(D_ALWAYS, "Error in removing job: %s from dap_queue\n", dap_id);
			dprintf(D_ALWAYS, "Still deleting job: %s from collection & logs\n", dap_id);
		}

		remove_credential (dap_id);
      
		std::string modify_s = "status = \"request_removed\"";
		write_collection_log(dapcollection, dap_id, modify_s.c_str());
	  
		char lognotes[MAXSTR] ;
		getValue(job_ad, "LogNotes", lognotes);
      
		dapcollection->RemoveClassAd(key);

		write_dap_log(historyfilename, "request_removed", 
					  "dap_id", dap_id, "error_code", "\"REMOVED!\"");
      
		write_xml_user_log(userlogfilename, "MyType", "\"JobAbortedEvent\"", 
						   "EventTypeNumber", "9", 
						   "TerminatedNormally", "0",
						   "Cluster", dap_id,
						   "Proc", "-1",
						   "Subproc", "-1",
						   "LogNotes", lognotes);
      

		adbuffer += "DaP job ";
		adbuffer += dap_id;
		adbuffer += " is removed from queue.";      
		rc = 1;
    }//if
    else {
		dprintf(D_ALWAYS, "Couldn't find/remove job: %s\n", dap_id);
		adbuffer += "Couldn't find/remove DaP job: ";
		adbuffer += dap_id;
		rc = 0;
    }
	if (dap_id) free (dap_id);
    
		//send response to the client
    sock->encode();
    char * pstr = strdup(adbuffer.c_str());
	if (pstr == NULL) {
		EXCEPT("Out of memory. Aborting.");
	}
    if (!(sock->code(rc) && sock->code (pstr))) {
		if (pstr) free (pstr);
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }
    if (pstr) free (pstr);
    
	return TRUE;
}

/* ============================================================================
 * shared dap reaper function
 * ==========================================================================*/
int dap_reaper(std::string modify_s, int pid,int exit_status)
{
	char dap_id[MAXSTR], dap_type[MAXSTR], reserve_id[MAXSTR];
	char fname[MAXSTR], linebuf[MAXSTR]="", linebufstr[MAXSTR] = "\"";
	FILE *f;
	classad::ClassAd *job_ad;
	std::string key = "key = ";
	char unstripped[MAXSTR];
  
	dprintf(D_ALWAYS, "Process %d terminated with exit status %d \n", 
			pid, exit_status);
	snprintf(fname, MAXSTR, "%s/out.%d", Log_dir, pid);

	if (dap_queue.get_dapid(pid, dap_id) != DAP_SUCCESS){
		dprintf(D_ALWAYS, "Process %d not in queue!\n", pid);
			//dap_queue.remove("0"); //just decrease num_jobs
		return DAP_ERROR;
	}
  
	dap_queue.remove(dap_id);

	key += dap_id;
	job_ad = dapcollection->GetClassAd(key);
	if (!job_ad) {
		return 0; //no such job!
	}

	getValue(job_ad, "dap_type", unstripped);
	strncpy(dap_type, strip_str(unstripped), MAXSTR);

	char lognotes[MAXSTR] ;
	getValue(job_ad, "LogNotes", lognotes);

		//---- if process completed succesfully ----
	if (exit_status == 0){

		remove_credential (dap_id);

		if (!strcmp(dap_type, "reserve")){
				//this part commented temporarily, required by nest_reserve
				/*
				  f = fopen(fname, "r");
				  fgets( linebuf, MAXSTR, f);

				  modify_s += "lot_id = ";
				  modify_s += linebuf;
				  modify_s += ";";
				*/
		}
    
		modify_s += "status = \"request_completed\"";
		write_collection_log(dapcollection, dap_id, modify_s.c_str());

			//-- remove the corresponding reserve request from collection
		if (!strcmp(dap_type, "release")){

			getValue(job_ad, "reserve_id", reserve_id);
			strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

			classad::LocalCollectionQuery query;
			classad::ClassAd       *job_ad2;
			char                    current_reserve_id[MAXSTR];
			char                    reserve_dap_id[MAXSTR];
			std::string             key2, constraint;
			classad::ClassAdParser  parser;

			constraint = "other.dap_type == \"reserve\"";
			classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
			if (!constraint_tree) {
				dprintf(D_ALWAYS, "Error in parsing constraint: %s\n", constraint.c_str());
			}

			query.Bind(dapcollection);  
			query.Query("root", constraint_tree);
			query.ToFirst();

			if ( query.Current(key2) ){
				do{
					job_ad2 = dapcollection->GetClassAd(key2);
					if (!job_ad2) {
						dprintf(D_ALWAYS, "No matching add!\n");
						break; //no matching classad
					}
					else{
						getValue(job_ad2, "reserve_id", current_reserve_id);
						strncpy(current_reserve_id, strip_str(current_reserve_id), MAXSTR);
	    
						if (!strcmp(current_reserve_id, reserve_id)) {
							getValue(job_ad2, "dap_id", reserve_dap_id);
							strncpy(reserve_dap_id, strip_str(reserve_dap_id), MAXSTR);
						}
					}
				}while (query.Next(key2));
			}
			if (constraint_tree != NULL) delete constraint_tree;

			key2 = "key = ";
			key2 += reserve_dap_id;
			dprintf(D_ALWAYS, "Reserve key: %s\n", reserve_dap_id);

			dapcollection->RemoveClassAd(key2);
		}//--



			//when request is comleted, remove it from the collection and 
			//log it to the history log file..
		if (!strcmp(dap_type, "reserve")){
				//this part commented temporarily, required by nest_reserve
				/*
				  fclose(f);
				  unlink(fname);
				*/
		}
		else{
			dapcollection->RemoveClassAd(key);
		}



		write_dap_log(historyfilename, "request_completed", "dap_id", dap_id);
			//    write_xml_log(xmllogfilename, job_ad, "\"request_completed\"");
    
		write_xml_user_log(userlogfilename, "MyType", "\"JobCompleteEvent\"", 
						   "EventTypeNumber", "5", 
						   "TerminatedNormally", "1",
						   "ReturnValue", "0",
						   "Cluster", dap_id,
						   "Proc", "-1",
						   "Subproc", "-1",
						   "LogNotes", lognotes);
    
	}

		//---- if process failed (exit_status != 0) ----
	else{
		int num_attempts; 
		char attempts[MAXSTR], tempstr[MAXSTR];
		char use_protocol[MAXSTR], alt_protocols[MAXSTR];
		int protocol_num, max_protocols = 0;

			//get num_attempts
		if (getValue(job_ad, "num_attempts", attempts) != DAP_SUCCESS)
			num_attempts = 0;
		else
			num_attempts = atoi(attempts);

		dprintf(D_ALWAYS, "number of attempts = %d\n", num_attempts);
    

		if ( (f = fopen(fname, "r")) == NULL ){
			dprintf(
				D_ALWAYS,
				"%s:%d: No error details for pid %d(file %s): (%d)%s\n",
				__FILE__, __LINE__,
				pid, fname,
				errno, strerror(errno)
			);
		}
		else{
			fgets( linebuf, MAXSTR, f);
		}

			//    string sstatus = "";
    
		if (num_attempts + 1 < (int)Max_retry){
				//sstatus = "\"request_rescheduled\"";
			modify_s += "status = \"request_rescheduled\";";
		}
    
		else{
				//      sstatus = "\"request_failed\"";
			modify_s += "status = \"request_failed\";";
		}
    
		snprintf(tempstr, MAXSTR, 
				 "num_attempts = %d;",num_attempts + 1);
    
		modify_s +=  tempstr;
		modify_s += "error_code = \"";
		modify_s += linebuf;
		modify_s += "\";";

			//get which protocol to use
		getValue(job_ad, "use_protocol", use_protocol);
		protocol_num = atoi(use_protocol);
    
		getValue(job_ad, "alt_protocols", alt_protocols);
    
		if (strcmp(alt_protocols,"")) {
			strtok(alt_protocols, ",");   
			max_protocols ++;
    
			while( strtok(NULL, ",") != NULL){
				max_protocols ++;
				dprintf(D_ALWAYS, "max protocols = %d\n", max_protocols);
			}
		}
    
			//set which protocol to use
		if (protocol_num < max_protocols){
			snprintf(tempstr, MAXSTR, "use_protocol = %d;",protocol_num + 1);
			modify_s +=  tempstr;
		}
		else{
			snprintf(tempstr, MAXSTR, "use_protocol = %d;",0);
			modify_s +=  tempstr;
		}
    
		write_collection_log(dapcollection, dap_id, modify_s.c_str());
			//    write_xml_log(xmllogfilename, job_ad, sstatus.c_str());
      
			//when request is failed for good, remove it from the collection and 
			//log it to the history log file..
		if (num_attempts + 1 >=  (int)Max_retry){
			strncat(linebufstr, linebuf, MAXSTR-2);
			strcat(linebufstr, "\"");

			dapcollection->RemoveClassAd(key);

			write_dap_log(historyfilename, "request_failed", 
						  "dap_id", dap_id, "error_code", linebufstr);
            
			char exit_status_str[MAXSTR];
			snprintf(exit_status_str, MAXSTR, "%d", exit_status);
      
			write_xml_user_log(userlogfilename, "MyType", "\"JobCompletedEvent\"", 
							   "EventTypeNumber", "5", 
							   "TerminatedNormally", "1",
							   "ReturnValue", exit_status_str,
							   "Cluster", dap_id,
							   "Proc", "-1",
							   "Subproc", "-1",
							   "LogNotes", lognotes);


			remove_credential (dap_id);
		}
      
		if (f != NULL){
			fclose(f);
			unlink(fname);
		}
    
	}//else
	return DAP_SUCCESS;
}

/* ============================================================================
 * transfer dap reaper function
 * ==========================================================================*/
int transfer_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

/* ============================================================================
 * reserve dap reaper function
 * ==========================================================================*/
int reserve_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
  
}

/* ============================================================================
 * release dap reaper function
 * ==========================================================================*/
int release_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

/* ============================================================================
 * requestpath dap reaper function
 * ==========================================================================*/
int requestpath_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

void remove_credential (char * dap_id) {
	char * filename = get_credential_filename (dap_id);

		// Switch to root
	priv_state old_priv = set_root_priv();
  
	struct stat stat_buff;

	if (stat ((const char*)filename, &stat_buff) == 0) {  
		int rc = unlink (filename);

		set_priv(old_priv);

		dprintf (D_FULLDEBUG, "Removing credential %s (rc=%d)\n", filename, rc);
	} else {
		set_priv(old_priv);
	}

	if (filename)
		free (filename);
}

char * get_credential_filename (char * dap_id) {
	std::string buff;
	buff += Cred_tmp_dir;
	buff += "/";
	buff += "cred-";
	buff += dap_id;
	return strdup (buff.c_str());
}

int
get_cred_from_credd (const char * request, void *& buff, int & size) {
	dprintf (D_FULLDEBUG, "Requesting credential %s\n", request);
  
	priv_state priv = set_user_priv();

	Daemon my_credd (DT_CREDD, NULL, NULL);

	Sock * socket =
		my_credd.startCommand (CREDD_GET_CRED, Stream::reli_sock, 0);
	ReliSock * sock = (ReliSock*)socket;

	if (!sock) {
		set_priv (priv);
		dprintf (D_ALWAYS,
			"%s:%d: Unable to start CREDD_GET_CRED command (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
		return FALSE;
	}

	
	// Authenticate
	if (!sock->isAuthenticated()) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS",
				"CLIENT");
		MyString methods;
		if (p) {
		  methods = p;
		  free (p);
		} else {
		  methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! sock->authenticate(methods.Value(), &errstack) ) {
		  dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
		  return FALSE;
		}
	}

	sock->encode();

	char * _request=strdup(request);
	if (!sock->code (_request) ) {
		dprintf(
			D_ALWAYS,
			"%s:%d: put to credd:  (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
	}
	free (_request);

	if (!sock->eom() ) {
		dprintf(
			D_ALWAYS,
			"%s:%d: put eom to credd:  (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
	}

	sock->decode();

	int rc = 0;
	if (!sock->code (size)) {
		dprintf(
			D_ALWAYS,
			"%s:%d: get cred size %d of %d bytes from credd:  (%d)%s\n",
			__FILE__, __LINE__,
			size, sizeof( size),
			errno, strerror(errno)
		);
	} else {
		int nbytes;
		buff = malloc (size);
		nbytes = sock->code_bytes (buff, size);
		if ( nbytes == size ) {
			dprintf (D_FULLDEBUG, "Received credential\n");
			rc = 1;
		}
		else {
			dprintf (
				D_ALWAYS,
				"%s:%d: ERROR recv'ing credential %s, %d of %d bytes (%d)%s\n",
				__FILE__, __LINE__,
				request,
				nbytes, size,
				errno, strerror(errno)
			);
		}
	}

	sock->close();
	delete sock;
	set_priv (priv);
	return (rc==1)?TRUE:FALSE;
}

int
init_user_id_from_FQN (const char * _fqn) {

	char * uid = NULL;
	char * domain = NULL;
	char * fqn = NULL;
  
	if (_fqn) {
		fqn = strdup (_fqn);
		uid = fqn;

			// Domain?
		char * pAt = strchr (fqn, '@');
		if (pAt) {
			*pAt='\0';
			domain = pAt+1;
		}
	}
  
	if (uid == NULL) {
		uid = "nobody";
	}

	int rc = init_user_ids (uid, domain);
	dprintf (D_FULLDEBUG, "Switching to user %s@%s, result = %d\n", uid, domain, rc);

	if (fqn)
		free (fqn);

	return rc;
}









