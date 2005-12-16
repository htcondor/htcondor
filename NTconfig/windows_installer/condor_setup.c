#include <stdio.h>
#include <errno.h>
#define CONDOR_COMMON_H
#include "getopt.h"

#ifdef WIN32
#define snprintf _snprintf
#define tempnam  _tempnam
#define strcasecmp stricmp
#endif
#define BUFFERSIZE (1024*5)
typedef enum { false=0, true=1 } bool;

struct Options {
	char newpool;    /* Y/N */
	char submitjobs; /* Y/N */
	char runjobs;    /* N/A/I/C (Never/Always/Idle/Cpu+Idle */
	char vacatejobs; /* Y/N */

	char *poolhostname;
	char *poolname; 
	char *jvmlocation;
	char *accountingdomain;
	char *release_dir;
	char *smtpserver;
	char *condoremail;
	char *hostallowread;
	char *hostallowwrite;
	char *hostallowadministrator;
} Opt = { '\0', '\0', '\0', '\0', NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL};

const char *short_options = ":c:d:e:i:j:v:n::p:o:r:a:s::t:m:h";
static struct option long_options[] =
{ 
	{"acctdomain",              optional_argument, 0, 'a'},
	{"hostallowread",           required_argument, 0, 'e'},
	{"hostallowwrite",          required_argument, 0, 't'},
	{"hostallowadministrator",  required_argument, 0, 'i'},
	{"newpool",                 optional_argument, 0, 'n'},
	{"runjobs",                 required_argument, 0, 'r'},
	{"vacatejobs",              optional_argument, 0, 'v'},
	{"submitjobs",              optional_argument, 0, 's'},
	{"condoremail",             required_argument, 0, 'c'},
	{"jvmlocation",             required_argument, 0, 'j'},
	{"poolname",                required_argument, 0, 'p'},
	{"poolhostname",            required_argument, 0, 'o'},
	{"release_dir",             required_argument, 0, 'd'},
	{"smtpserver",              required_argument, 0, 'm'},
	{"help",                    no_argument,       0, 'h'},
   	{0, 0, 0, 0}
};
	
const char *Config_file = NULL;


bool parse_args(int argc, char** argv);
bool append_option(const char *attribute, const char *val);
bool set_option(const char *attribute, const char *val);
bool set_int_option(const char *attribute, const int val);
bool setup_config_file(void) { Config_file = "condor_config"; return true; };
bool isempty(const char * a) { return ((a == NULL) || (a[0] == '\0')); }
bool attribute_matches( const char *string, const char *attr );

void Usage();

void set_release_dir();
void set_daemonlist();
void set_poolinfo();
void set_startdpolicy();
void set_jvmlocation();
void set_mailoptions();
void set_hostpermissions();

int 
main(int argc, char**argv) {

	// parse arguments
	
	int i;
	if (! parse_args(argc, argv) ) {
		exit(1);
	}

	set_release_dir();
	set_daemonlist();
	set_poolinfo(); /* name; hostname */
	set_startdpolicy();
	set_jvmlocation();
	set_mailoptions();
	set_hostpermissions();

//	getc(stdin);
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
		snprintf(buf, 1024, "MASTER %s %s %s", 
				(Opt.newpool == 'Y') ? "COLLECTOR NEGOTIATOR" : "",
				(Opt.submitjobs == 'Y') ? "SCHEDD" : "",
				(Opt.runjobs == 'A' ||
				 Opt.runjobs == 'I' ||
				 Opt.runjobs == 'C') ? "STARTD" : "");

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
				if (!isempty(optarg)) {
					Opt.accountingdomain = strdup(optarg);
				}
			break;
			case 'c':
				if (!isempty(optarg)) {
					Opt.condoremail = strdup(optarg);
				}
			break;

			case 'e':
				if (!isempty(optarg)) {
					Opt.hostallowread = strdup(optarg);
				}
			break;

			case 't':
				if (!isempty(optarg)) {
					Opt.hostallowwrite = strdup(optarg);
				}
			break;

			case 'i':
				if (!isempty(optarg)) {
					Opt.hostallowadministrator = strdup(optarg);
				}
			break;

			case 'j':
				if (!isempty(optarg)) {
					Opt.jvmlocation = strdup(optarg);
				}
			break;

			case 'n': 
				if ( optarg && (optarg[0] == 'N' || optarg[0] == 'n') ) {
					Opt.newpool = 'N';
				} else {
					Opt.newpool = 'Y';
				}
			break;
			
			case 'd':
				if (!isempty(optarg)) {
					Opt.release_dir = strdup(optarg);
				}
			break;

			case 'p':
				if (!isempty(optarg)) {
					Opt.poolname = strdup(optarg);
				}
			break;

			case 'o':
				if (!isempty(optarg)) {
					Opt.poolhostname = strdup(optarg);
				}
			break;
			
			case 'r':
				Opt.runjobs = toupper(optarg[0]);
			break;
			
			case 's':
				if ( optarg && (optarg[0] == 'N' || optarg[0] == 'n') ) {
					Opt.submitjobs = 'N';
				} else {
					Opt.submitjobs = 'Y';
				}
			break;
			
			case 'v':
				if ( optarg && (optarg[0] == 'N' || optarg[0] == 'n') ) {
					Opt.vacatejobs = 'N';
				} else {
					Opt.vacatejobs = 'Y';
				}
			break;

			case 'm':
				if (!isempty(optarg)) {
					Opt.smtpserver = strdup(optarg);
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
		fprintf(stderr, "%s: --newpool, --runjobs and --submitjobs must be\n"
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
		fprintf(stderr, "Error opening config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		result = false;
		return result;
	}

	if ( NULL == ( config_file_tmp = tempnam(".", Config_file)) ) {
		fprintf(stderr, "Error creating temporary file '%s'.\n\tErr=%s\n",
				config_file_tmp, strerror(errno));
		result = false;
		return result;
	} else {
		 fprintf(stderr, "tmp file: %s\n", config_file_tmp);
	}

	cfg_out = fopen(config_file_tmp, "wb");
	if ( cfg_out == NULL ) {
		fprintf(stderr, "Error opening config file '%s'.\n\tErr=%s\n",
				config_file_tmp, strerror(errno));
		result = false;
		return result;
	}

	cfg = fopen(Config_file, "rb");

	if ( cfg == NULL ) {
		fprintf(stderr, "Error opening config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		fclose(cfg_out);
		result = false;
		return result;
	}

	/* seek to beginning of the file */
	if ( 0 != fseek(cfg, 0, SEEK_SET) ) {
		fprintf(stderr, "Error seeking config file '%s'.\n\tErr=%s\n",
				Config_file, strerror(errno));
		result = false;
		return result;
	}
	

	while (1) {
		if ( NULL == fgets(buf, BUFFERSIZE, cfg) ) {
			if (feof(cfg)) {
	
				/* reached end of file */
				if (0 != fclose(cfg)) {
					fprintf(stderr, "Error closing config file '%s'.\n"
							"\tErr=%s\n", Config_file, strerror(errno));
					result = false;
				}

				if ( !foundit ) {
					/* new option, so append it to the file */
					fflush(cfg_out);
					if ( 0 == fprintf(cfg_out, "%s = %s\n", attribute, val) ) {
						fprintf(stderr, "Error appending to config file '%s'.\n"
							"\tErr=%s\n", config_file_tmp, strerror(errno));
						result = false;
					}
				}

				if (0 != fclose(cfg_out)) {
					fprintf(stderr, "Error closing temp config file '%s'.\n"
							"\tErr=%s\n", config_file_tmp, strerror(errno));
					result = false;
				}

				if (0 != remove(Config_file) ) {
					fprintf(stderr, "Error removing old config file '%s'.\n"
							"\tErr=%s\n", Config_file, strerror(errno));
				} else if (0 != rename(config_file_tmp, Config_file) ) {
					fprintf(stderr, "Error moving new config file '%s' to "
							"'%s'.\n\tErr=%s\n", config_file_tmp, Config_file,
						   	strerror(errno));
				}
				break;

			} else {
				fprintf(stderr, "Error reading config file '%s'.\n\tErr=%s\n",
					Config_file, strerror(errno));
			}
			return result;
		}

		if ( !foundit && attribute_matches(buf, attribute) ) {
			fflush(cfg_out);
			if (0 == fprintf(cfg_out, "%s = %s\n", attribute, val)) {
				fprintf(stderr, "Error writing to config file '%s'.\n"
					"\tErr=%s\n", config_file_tmp, strerror(errno));
				result = false;
			}
			foundit = true;
		} else {
				// no change
			if ( 0 == fprintf(cfg_out, "%s", buf) ) {
				fprintf(stderr, "Error writing to config file '%s'.\n"
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
		fprintf(stderr, "Error setting option '%s' to '%d'.\n\t"
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
