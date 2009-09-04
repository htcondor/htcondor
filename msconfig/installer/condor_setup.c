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


#include <windows.h>
#include <stdio.h>
#include <errno.h>
#define CONDOR_COMMON_H
#include "my_getopt.h"

#ifdef WIN32
#define snprintf _snprintf
#define tempnam  _tempnam
#define strcasecmp stricmp
#endif
#define BUFFERSIZE (1024*5)
typedef enum { false=0, true=1 } bool;

/* get rid of some boring warnings */
#define UNUSED_VARIABLE(x) x;

struct Options {
	char newpool;    /* Y/N */
	char submitjobs; /* Y/N */
	char runjobs;    /* N/A/I/C (Never/Always/Idle/Cpu+Idle */
	char vacatejobs; /* Y/N */
	char enablevmuniverse; /* Y/N */
	char vmnetworking; /* N/A/B/C (None/NAT/Bridge/NAT and Bridge) */
	char hadoop;     /* N/Y */

	char *poolhostname;
	char *poolname; 
	char *jvmlocation;
	char *perllocation;
	char *accountingdomain;
	char *release_dir;
	char *smtpserver;
	char *condoremail;
	char *hostallowread;
	char *hostallowwrite;
	char *hostallowadministrator;
	char *vmmaxnumber;
	char *vmversion;
	char *vmmemory;
	char *namedata;
	char *namenode;
	char *nameport;
	char *namewebport;
} Opt = { '\0', '\0', '\0', '\0', '\0','\0', '\0', NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

const char *short_options = ":c:d:e:i:j:v:n:p:o:r:a:s:t:m:u:l:w:x:y:z:q:f:k:g:b:h";
static struct option long_options[] =
{ 
	{"acctdomain",              required_argument, 0, 'a'},
	{"hostallowread",           required_argument, 0, 'e'},
	{"hostallowwrite",          required_argument, 0, 't'},
	{"hostallowadministrator",  required_argument, 0, 'i'},
	{"newpool",                 required_argument, 0, 'n'},
	{"runjobs",                 required_argument, 0, 'r'},
	{"vacatejobs",              required_argument, 0, 'v'},
	{"submitjobs",              required_argument, 0, 's'},
	{"condoremail",             required_argument, 0, 'c'},
	{"jvmlocation",             required_argument, 0, 'j'},
	{"perllocation",            required_argument, 0, 'l'},
	{"poolname",                required_argument, 0, 'p'},
	{"poolhostname",            required_argument, 0, 'o'},
	{"release_dir",             required_argument, 0, 'd'},
	{"smtpserver",              required_argument, 0, 'm'},
	{"enablevmuniverse",        required_argument, 0, 'u'},
	{"vmmaxnumber",             required_argument, 0, 'w'},	
	{"vmversion",               required_argument, 0, 'x'},
	{"vmmemory",                required_argument, 0, 'y'},
	{"vmnetworking",            required_argument, 0, 'z'},
	{"hadoop",					required_argument, 0, 'q'},
	{"namenode",				required_argument, 0, 'f'},
	{"namedata",				required_argument, 0, 'k'},
	{"nameport",				required_argument, 0, 'g'},
	{"namewebport",				required_argument, 0, 'b'},
	{"help",                    no_argument,       0, 'h'},
   	{0, 0, 0, 0}
};

const char *Config_file = NULL;
const char *LogFileName = "install.log";
FILE *LogFileHandle = NULL;


bool parse_args(int argc, char** argv);
bool append_option(const char *attribute, const char *val);
bool set_option(const char *attribute, const char *val);
bool set_int_option(const char *attribute, const int val);
bool setup_config_file(void) { Config_file = "condor_config"; return true; };
bool open_log_file(void) { return ( NULL != ( LogFileHandle = fopen(LogFileName, "w") ) ); }
bool close_log_file(void) { if ( NULL != LogFileHandle ) { fclose(LogFileHandle); LogFileHandle = NULL; } return true; }
bool isempty(const char * a) { return ((a == NULL) || (a[0] == '\0')); }
bool attribute_matches( const char *string, const char *attr );
char* get_short_path_name (const char *path);

void Usage();

void set_release_dir();
void set_daemonlist();
void set_poolinfo();
void set_startdpolicy();
void set_jvmlocation();
void set_vmgahpoptions();
void set_mailoptions();
void set_hostpermissions();
void set_vmuniverse();
void set_hdfs();

int WINAPI WinMain( HINSTANCE hInstance, 
		    HINSTANCE hPrevInstance, 
		    LPSTR     lpCmdLine, 
		    int       nShowCmd ) {

  UNUSED_VARIABLE ( hInstance );
  UNUSED_VARIABLE ( hPrevInstance );
  UNUSED_VARIABLE ( lpCmdLine );
  UNUSED_VARIABLE ( nShowCmd ); 
  
  open_log_file();
  
 
  if ( !parse_args ( __argc, __argv ) ) {
    exit ( 1 );
  }
  
  set_release_dir ();
  set_daemonlist ();
  set_poolinfo (); /* name; hostname */
  set_startdpolicy ();
  set_jvmlocation ();
  
  set_mailoptions ();
  set_hostpermissions ();
  set_vmuniverse();
  set_hdfs();
  
  /* the following options go in the vmgahp config file */
  if ( 'Y' == Opt.enablevmuniverse ) {
    set_vmgahpoptions ();
  }
  
  close_log_file ();
  
  // getc ( stdin );
  return 0;

}

void
Usage() {

	int i;

	printf("Usage:\n  condor_setup [options]\n\n");
	for (i=0; long_options[i].name; i++) {
		printf("  -%c, --%-12s\n", long_options[i].val, 
			long_options[i].name);
	}
	exit(0);
}

void 
set_mailoptions() {

	if ( Opt.smtpserver) {
		set_option("SMTP_SERVER", Opt.smtpserver);
	}

	if ( Opt.condoremail ) {
		set_option("CONDOR_ADMIN", Opt.condoremail);
	}

	if ( Opt.accountingdomain && Opt.accountingdomain[0] != '\0' ) {
		set_option("UID_DOMAIN", Opt.accountingdomain);
	}
}

void 
set_jvmlocation() {

	if ( Opt.jvmlocation ) {
		set_option("JAVA", Opt.jvmlocation);
	}
}

void
set_vmgahpoptions() {

	if ( Opt.perllocation ) {
		set_option("VMWARE_PERL", Opt.perllocation);
	}

	if ( Opt.release_dir ) {
		char *control_script = "bin\\condor_vm_vmware.pl";
 
		char *tmp = malloc(strlen(Opt.release_dir) 
				   + strlen(control_script) + 2); 
		char *short_name = NULL;
		sprintf(tmp, "%s\\%s", Opt.release_dir, control_script);
		short_name = get_short_path_name(tmp);
 
		free(tmp);
 
		set_option("VMWARE_SCRIPT", short_name);
		free(short_name);
 
	}

	if ( Opt.vmversion ) {
		set_option("VM_VERSION", Opt.vmversion);
	}

	if ( Opt.vmmemory ) {
		set_option("VM_MEMORY", Opt.vmmemory);
	}

	set_option("VM_NETWORKING", "TRUE");
	if ( 'A' == Opt.vmnetworking ) {
		set_option("VM_NETWORKING_TYPE", "nat");
	} else if ( 'B' == Opt.vmnetworking ) {
		set_option("VM_NETWORKING_TYPE", "bridge");
	} else if ( 'C' == Opt.vmnetworking ) {
		set_option("VM_NETWORKING_TYPE", "nat, bridge");
	} else { /* no networking */ 
		set_option("VM_NETWORKING", "FALSE");
	}

}

void
set_release_dir() {
	if ( Opt.release_dir ) {
		set_option("RELEASE_DIR", Opt.release_dir);
		set_option("LOCAL_DIR", Opt.release_dir);
	}
}

void
set_startdpolicy() {
	if (Opt.runjobs == 'A') {
			/* always run jobs */
		set_option("START", "TRUE");
		set_option("SUSPEND", "FALSE");
		set_option("WANT_SUSPEND", "FALSE");
		set_option("WANT_VACATE", "FALSE");
		set_option("PREEMPT", "FALSE");

	} else if (Opt.runjobs == 'N') {
			/* never run jobs */
		set_option("START", "FALSE");

	} else if (Opt.runjobs == 'I') {
			/* only run jobs when console is idle */
		set_option("START", "KeyboardIdle > $(StartIdleTime)");
		set_option("SUSPEND", "$(UWCS_SUSPEND)");
		set_option("CONTINUE", "KeyboardIdle > $(ContinueIdleTime)");

	} else if (Opt.runjobs == 'C') {
			/* only run jobs when idle and low cpu */
		set_option("START", "$(UWCS_START)");
		set_option("CONTINUE", "$(UWCS_CONTINUE)");

		set_option("SUSPEND", "$(UWCS_SUSPEND)");
		set_option("WANT_SUSPEND", "$(UWCS_WANT_SUSPEND)");
		set_option("WANT_VACATE", "$(UWCS_WANT_VACATE)");
		set_option("WANT_PREEMPT", "$(UWCS_WANT_PREEMPT)");
	}

	if ( Opt.vacatejobs == 'N' ) {
	 	set_option("WANT_VACATE", "FALSE");
	 	set_option("WANT_SUSPEND", "TRUE");
	} else if ( Opt.vacatejobs == 'Y' ) {
	 	set_option("WANT_VACATE", "$(UWCS_WANT_VACATE)");
	 	set_option("WANT_SUSPEND", "$(UWCS_WANT_SUSPEND)");
	}

}

void 
set_poolinfo() {
	if ( Opt.poolname != NULL ) {
		set_option("COLLECTOR_NAME", Opt.poolname);
		set_option("CONDOR_HOST", "$(FULL_HOSTNAME)");
	}

	if ( Opt.poolhostname != NULL && Opt.poolhostname[0] != '\0' ) {
		set_option("CONDOR_HOST", Opt.poolhostname);
	}

}

void
set_daemonlist() {
	char buf[1024];

	if ( Opt.newpool && Opt.submitjobs && Opt.runjobs ) {
		snprintf(buf, 1024, "MASTER %s %s %s %s", 
				(Opt.newpool == 'Y') ? "COLLECTOR NEGOTIATOR" : "",
				(Opt.submitjobs == 'Y') ? "SCHEDD" : "",
				(Opt.runjobs == 'A' ||
				 Opt.runjobs == 'I' ||
				 Opt.runjobs == 'C') ? "STARTD" : "",
				(Opt.hadoop == 'Y') ? "HDFS" : "");

		set_option("DAEMON_LIST", buf);
	}
}

void
set_hostpermissions() {
	if ( Opt.hostallowread != NULL ) {
		set_option("HOSTALLOW_READ", Opt.hostallowread);
	}
	if ( Opt.hostallowwrite != NULL ) {
		set_option("HOSTALLOW_WRITE", Opt.hostallowwrite);
	}
	if ( Opt.hostallowadministrator != NULL ) {
		set_option("HOSTALLOW_ADMINISTRATOR", Opt.hostallowadministrator);
	}
}

void
set_vmuniverse() {
	if ( Opt.enablevmuniverse == 'Y' ) {
		set_option("VM_GAHP_SERVER", "$(BIN)/condor_vm-gahp.exe");
		set_option("VM_TYPE", "vmware");
	}

	if ( Opt.vmmaxnumber ) {
		set_option("VM_MAX_NUMBER", Opt.vmmaxnumber);
	}

	if ( 'N' == Opt.vmnetworking ) {
		set_option("VM_NETWORKING", "FALSE");
	} else {
		set_option("VM_NETWORKING", "TRUE");	  
	}
	
}

void set_hdfs() {
	char buf[MAX_PATH];
	if ( Opt.namedata ) {
		set_option("HDFS_SERVICES", Opt.namedata);
		set_option("HDFS_NAMENODE_DIR", "$(RELEASE_DIR)/HDFS/hadoop_name");
		set_option("HDFS_DATANODE_DIR", "$(RELEASE_DIR)/HDFS/hadoop_data");
		set_option("HDFS_HOME", "$(RELEASE_DIR)/HDFS");
	}

	if ( Opt.namenode && Opt.nameport ) {
		snprintf(buf, MAX_PATH, "%s%s%s", Opt.namenode, ":", Opt.nameport);
		set_option("HDFS_NAMENODE", buf);
	}

	if ( Opt.namenode && Opt.namewebport ) {
		snprintf(buf, MAX_PATH, "%s%s%s", Opt.namenode, ":", Opt.namewebport);
		set_option("HDFS_NAMENODE_WEB", buf);
	}
}

bool 
parse_args(int argc, char** argv) {

	int i, option_index;
	if ( argc < 2 ) {
		Usage();
	}
	while ( (i = my_getopt_long(argc, argv, short_options,
					long_options, &option_index)) > 0 ) {

		switch(i) {
			case 'a':
				if (!isempty(my_optarg)) {
					Opt.accountingdomain = strdup(my_optarg);
				}
			break;
			case 'c':
				if (!isempty(my_optarg)) {
					Opt.condoremail = strdup(my_optarg);
				}
			break;

			case 'e':
				if (!isempty(my_optarg)) {
					Opt.hostallowread = strdup(my_optarg);
				}
			break;

			case 't':
				if (!isempty(my_optarg)) {
					Opt.hostallowwrite = strdup(my_optarg);
				}
			break;

			case 'i':
				if (!isempty(my_optarg)) {
					Opt.hostallowadministrator = strdup(my_optarg);
				}
			break;

			case 'j':
				if (!isempty(my_optarg)) {
					Opt.jvmlocation = get_short_path_name(my_optarg);
				}
			break;

			case 'l':
				if (!isempty(my_optarg)) {
					Opt.perllocation = get_short_path_name(my_optarg);
				}
			break;

			case 'n': 
				if ( my_optarg && (my_optarg[0] == 'N' || my_optarg[0] == 'n') ) {
					Opt.newpool = 'N';
				} else {
					Opt.newpool = 'Y';
				}
			break;
			
			case 'd':
				if (!isempty(my_optarg)) {
					Opt.release_dir = get_short_path_name(my_optarg);
				}
			break;

			case 'p':
				if (!isempty(my_optarg)) {
					Opt.poolname = strdup(my_optarg);
				}
			break;

			case 'o':
				if (!isempty(my_optarg)) {
					Opt.poolhostname = strdup(my_optarg);
				}
			break;
			
			case 'r':
				Opt.runjobs = toupper(my_optarg[0]);
			break;
			
			case 's':
				if ( my_optarg && (my_optarg[0] == 'N' || my_optarg[0] == 'n') ) {
					Opt.submitjobs = 'N';
				} else {
					Opt.submitjobs = 'Y';
				}
			break;
			
			case 'v':
				if ( my_optarg && (my_optarg[0] == 'N' || my_optarg[0] == 'n') ) {
					Opt.vacatejobs = 'N';
				} else {
					Opt.vacatejobs = 'Y';
				}
			break;

			case 'm':
				if (!isempty(my_optarg)) {
					Opt.smtpserver = strdup(my_optarg);
				}
			break;

			case 'u':
				if ( my_optarg && (my_optarg[0] == 'N' || my_optarg[0] == 'n') ) {
					Opt.enablevmuniverse = 'N';
				} else {
					Opt.enablevmuniverse = 'Y';
				}
			break;
			
			case 'w':
				if (!isempty(my_optarg)) {
					Opt.vmmaxnumber = strdup(my_optarg);
				}
			break;

			case 'x':
				if (!isempty(my_optarg)) {
					Opt.vmversion = strdup(my_optarg);
				}
			break;

			case 'y':
				if (!isempty(my_optarg)) {
					Opt.vmmemory = strdup(my_optarg);
				}
			break;

			case 'z':
				if ( my_optarg ) {
					Opt.vmnetworking = toupper(my_optarg[0]);
				}
			break;

			case 'q':
				if ( my_optarg ) {
					Opt.hadoop = toupper(my_optarg[0]);
				}
				break;

			case 'f':
				if( !isempty(my_optarg) ) {
					Opt.namenode = strdup(my_optarg);
				}
				break;

			case 'k':
				if( !isempty(my_optarg) ) {
					Opt.namedata = strdup(my_optarg);
				}
				break;

			case 'g':
				if( !isempty(my_optarg) ) {
					Opt.nameport = strdup(my_optarg);
				}
				break;

			case 'b':
				if( !isempty(my_optarg) ) {
					Opt.namewebport = strdup(my_optarg);
				}
				break;
			
			case 'h':
			default:
				/* getopt already printed an error msg */
				Usage();
		}
	}
	

	
/*	printf("Opt.newpool = %c\n"
	"Opt.submitjobs = %c\n"
	"Opt.runjobs = %c\n"
	"Opt.vacatejobs = %c\n"
	"Opt.poolhostname = %s\n"
	"Opt.poolname = %s\n"
	"Opt.jvmlocation = %s\n"
	"Opt.smtpserver = %s\n"
	"Opt.condoremail = %s\n", Opt.newpool, Opt.submitjobs, Opt.runjobs, 
	Opt.vacatejobs,
	Opt.poolhostname, Opt.poolname, Opt.jvmlocation, Opt.smtpserver,
	Opt.condoremail);
*/
	if (!((Opt.submitjobs && Opt.runjobs && Opt.newpool ) ||
	      (!Opt.submitjobs && !Opt.runjobs && !Opt.newpool ) )) {
		fprintf(LogFileHandle, "%s: --newpool, --runjobs and --submitjobs must be\n"
				"\tspecified together\n", argv[0]);
		return false;
	}
	return true;
}

bool
set_option(const char *attribute, const char *val) {
	char buf[BUFFERSIZE];
	char *config_file_tmp;
	FILE *cfg, *cfg_out;
	bool result, foundit;

	result        = true;
	foundit       = false;
	cfg = cfg_out = NULL;


	if ( Config_file == NULL && !setup_config_file() ) {
		fprintf(LogFileHandle, "Error opening config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		result = false;
		return result;
	}

	if ( NULL == ( config_file_tmp = tempnam(".", Config_file)) ) {
		fprintf(LogFileHandle, "Error creating temporary file '%s'.\n\tErr=%s\n",
				config_file_tmp, strerror(errno));
		result = false;
		return result;
	} else {
		// fprintf(LogFileHandle, "tmp file: %s\n", config_file_tmp);
	}

	cfg_out = fopen(config_file_tmp, "w");
	if ( cfg_out == NULL ) {
		fprintf(LogFileHandle, "Error opening config file '%s'.\n\tErr=%s\n",
				config_file_tmp, strerror(errno));
		result = false;
		return result;
	}

	cfg = fopen(Config_file, "r");

	if ( cfg == NULL ) {
		fprintf(LogFileHandle, "Error opening config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		fclose(cfg_out);
		result = false;
		return result;
	}

	/* seek to beginning of the file */
	if ( 0 != fseek(cfg, 0, SEEK_SET) ) {
		fprintf(LogFileHandle, "Error seeking config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		result = false;
		return result;
	}
	

	while (1) {
		if ( NULL == fgets(buf, BUFFERSIZE, cfg) ) {
			if (feof(cfg)) {
	
				/* reached end of file */
				if (0 != fclose(cfg)) {
					fprintf(LogFileHandle, "Error closing config file '%s'.\n"
							"\tErr=%s\n", Config_file, strerror(errno));
					result = false;
				}

				if ( !foundit ) {
					/* new option, so append it to the file */
					fflush(cfg_out);
					if ( 0 == fprintf(cfg_out, "%s = %s\n", attribute, val) ) {
						fprintf(LogFileHandle, "Error appending to config file '%s'.\n"
							"\tErr=%s\n", config_file_tmp, strerror(errno));
						result = false;
					}
				}

				if (0 != fclose(cfg_out)) {
					fprintf(LogFileHandle, "Error closing temp config file '%s'.\n"
							"\tErr=%s\n", config_file_tmp, strerror(errno));
					result = false;
				}

				if (0 != remove(Config_file) ) {
					fprintf(LogFileHandle, "Error removing old config file '%s'.\n"
							"\tErr=%s\n", Config_file, strerror(errno));
				} else if (0 != rename(config_file_tmp, Config_file) ) {
					fprintf(LogFileHandle, "Error moving new config file '%s' to "
							"'%s'.\n\tErr=%s\n", config_file_tmp, Config_file,
						   	strerror(errno));
				}
				break;

			} else {
				fprintf(LogFileHandle, "Error reading config file '%s'.\n\tErr=%s\n",
					Config_file, strerror(errno));
			}
			return result;
		}

		if ( !foundit && attribute_matches(buf, attribute) ) {
			fflush(cfg_out);
			if (0 == fprintf(cfg_out, "%s = %s\n", attribute, val)) {
				fprintf(LogFileHandle, "Error writing to config file '%s'.\n"
					"\tErr=%s\n", config_file_tmp, strerror(errno));
				result = false;
			}
			foundit = true;
		} else {
				// no change
			if ( 0 == fprintf(cfg_out, "%s", buf) ) {
				fprintf(LogFileHandle, "Error writing to config file '%s'.\n"
					"\tErr=%s\n", config_file_tmp, strerror(errno));
				result = false;
			}
		}

	}
	return result;
}

bool
set_int_option(const char *attribute, const int val) {
	char buf[64];

	if ( 0 > snprintf(buf, 63, "%d", val) ) {
		fprintf(LogFileHandle, "Error setting option '%s' to '%d'.\n\t"
			   "Integer too big!!\n", attribute, val);
		return false;
	} else {
		return set_option(attribute, buf);
	}
}

bool
append_option(const char *attribute, const char *val) {
	return false;
}

bool
attribute_matches( const char *string, const char *attr ) {

	int i, j, attr_len;
	char *buf;
	bool matches;

	matches = false;
	i = j = 0;
	attr_len = strlen(string);
	buf = (char*)malloc((attr_len+1)*sizeof(char));

	for (; string[i]; i++)  {

			/* stop parsing attribute name if we hit any of the following */
		if ( ( string[i] == '\n') ||

				/* comment */
			( string[i] == '#') ||

				/* assignment */
			( string[i] == '=') ||

				/* line continuation */
			( (string[i] == '\\') && (string[i+1] == '\n') ) ||

				/* trailing whitespace */
			( (string[i] == ' ') && (j>0) ) ||
			( (string[i] == '\t') && (j>0) ) 
			) {
			 break;
		}

		/* chew up leading whitespace */
		if ( string[i] == ' ' ) { continue; }
		if ( string[i] == '\t' ) { continue; }

		buf[j] = string[i];
		j++;
	}

	buf[j] = '\0';

//	printf("parsed: '%s' \t given: '%s'\n", buf, attr);


	matches = ( 0 == strcasecmp(attr, buf) );
	free(buf);

	return matches;
}

/* ugly hack to get short paths into the config file, because
   we do not support spaces in paths in all places */		
char* 
get_short_path_name(const char *path) {
	char *short_path = (char*)malloc(MAX_PATH * sizeof(char));
	if (short_path) {
		if (GetShortPathName(path, short_path, MAX_PATH) > 0) {
			return short_path;
		}
		/* may fail because the path has unexpanded variables,
		   so we can't just assume it's an error... */
		free(short_path);
	}
	return strdup(path);
}
