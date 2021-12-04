/*
#  File:     blparser_master.c 
#
#  Author:   Massimo Mezzadri
#  e-mail:   Massimo.Mezzadri@mi.infn.it
# 
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# See http://www.eu-egee.org/partners/ for details on the copyright
# holders.  
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
# 
#     http://www.apache.org/licenses/LICENSE-2.0 
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
# 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#ifdef MTRACE_ON
#include <mcheck.h>
#endif
#include <pthread.h>
#include <wordexp.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>

#include "blahpd.h"
#include "config.h"
#include "blah_utils.h"

#define CONFIG_FILE_PARSER "blparser.conf"
#define PID_DIR "/var/run"
#define MAXPARSERNUM 30
#define PARSER_COMMAND_LINE_SIZE 500
#define PID_FILENAME_SIZE 200

/* Global variables */
struct blah_managed_child {
	char *exefile;
	char *pidfile;
};
static struct blah_managed_child *parser_pbs=NULL;
static struct blah_managed_child *parser_lsf=NULL;

int usepbs=0;
int uselsf=0;
int parsernumpbs=0;
int parsernumlsf=0;

static pthread_mutex_t bfork_lock  = PTHREAD_MUTEX_INITIALIZER;

extern config_handle *blah_config_handle;
config_entry *ret;

char *config_file=NULL;

void check_on_children_args(const struct blah_managed_child *children, const int count);
void sigterm();
void daemonize();
int merciful_kill_noglexec(pid_t pid);

/* Functions */

/* Check on good health of our managed children */

void 
check_on_children_args(const struct blah_managed_child *children, const int count)
{
	FILE *pid;
        FILE *fpid;
	pid_t ch_pid;
	int junk;
	int i,j;
	int try_to_restart;
	int fret;
	static time_t lastfork=0;
	time_t new_lastfork;
	static time_t calldiff=0;
	const time_t default_calldiff = 5;
	config_entry *ccld;
	time_t now;

        wordexp_t args;
	
	pthread_mutex_lock(&bfork_lock);
	if (calldiff <= 0)
	{
		ccld = config_get("blah_children_restart_interval",blah_config_handle);
		if (ccld != NULL) calldiff = atoi(ccld->value);
		if (calldiff <= 0) calldiff = default_calldiff;
	}

	time(&now);
	new_lastfork = lastfork;

	for (i=0; i<count; i++)
	{
		try_to_restart = 0;
		if ((pid = fopen(children[i].pidfile, "r")) == NULL)
		{
			if (errno != ENOENT) continue;
			else try_to_restart = 1;
		} else {
			if (fscanf(pid,"%d",&ch_pid) < 1) 
			{
				fclose(pid);
				continue;
			}
			if (kill(ch_pid, 0) < 0)
			{
				/* The child process disappeared. */
				if (errno == ESRCH) try_to_restart = 1;
			}
			fclose(pid);
		}
		if (try_to_restart)
		{
			/* Don't attempt to restart too often. */
			if ((now - lastfork) < calldiff) 
			{
				fprintf(stderr,"Restarting %s too frequently.\n",
					children[i].exefile);
				fprintf(stderr,"Last restart %d seconds ago (<%d).\n",
					(int)(now-lastfork), (int)calldiff);
				continue;
			}
			fret = fork();
			if (fret == 0)
			{
				if((j = wordexp(children[i].exefile, &args, WRDE_NOCMD)))
				{
					fprintf(stderr,"wordexp: unable to parse the command line \"%s\" (error %d)\n", children[i].exefile, j);
					_exit(1);
				}
				/* Child process. Exec exe file. */
				if (execv(args.we_wordv[0], args.we_wordv) < 0)
				{
					fprintf(stderr,"Cannot exec %s: %s\n",
						children[i].exefile,
						strerror(errno));
					_exit(1);
				}
				/* Free the wordexp'd args */
 				wordfree(&args);

			} else if (fret < 0) {
				fprintf(stderr,"Cannot fork trying to start %s: %s\n",
					children[i].exefile,
					strerror(errno));
			} else {
				if (kill(fret, 0) < 0){
					/* The child process disappeared. */
					if (errno == ESRCH) try_to_restart = 1;
				}else{
        				fpid = fopen(children[i].pidfile, "w");
        				if ( !fpid ) { perror(children[i].pidfile); return; }
					if (fprintf(fpid, "%d", fret) <= 0) { perror(children[i].pidfile); return; }
        				if (fclose(fpid) != 0) { perror(children[i].pidfile); return; }
				}
			}
			new_lastfork = now;
		}
	}

	/* Reap dead children. Yuck.*/
	while (waitpid(-1, &junk, WNOHANG) > 0) /* Empty loop */;

	lastfork = new_lastfork;

	pthread_mutex_unlock(&bfork_lock);
}

void 
sigterm()
{
			
	int i;
	FILE *pid;
	pid_t ch_pid;

	if(usepbs){
		for(i=0;i<parsernumpbs;i++){
			if ((pid = fopen(parser_pbs[i].pidfile, "r")) != NULL){
				if (fscanf(pid,"%d",&ch_pid) < 1){
					fclose(pid);
					continue;
				}
			}
			fclose(pid);
			merciful_kill_noglexec(ch_pid);	
		}
	}
	if(uselsf){
		for(i=0;i<parsernumlsf;i++){
			if ((pid = fopen(parser_lsf[i].pidfile, "r")) != NULL){
				if (fscanf(pid,"%d",&ch_pid) < 1){
					fclose(pid);
					continue;
				}
			}
			fclose(pid);
			merciful_kill_noglexec(ch_pid);	
		}
	}
	exit(0);

}

void
daemonize()
{

        int pid;

        pid = fork();

        if (pid < 0){
		fprintf(stderr, "Cannot fork in daemonize\n");
                exit(EXIT_FAILURE);
        }else if (pid >0){
                exit(EXIT_SUCCESS);
        }

        setsid();

        pid = fork();

        if (pid < 0){
		fprintf(stderr, "Cannot fork in daemonize\n");
                exit(EXIT_FAILURE);
        }else if (pid >0){
                exit(EXIT_SUCCESS);
        }

        chdir("/");
        umask(0);

        freopen ("/dev/null", "r", stdin);
        freopen ("/dev/null", "w", stdout);
        freopen ("/dev/null", "w", stderr);

}

int
merciful_kill_noglexec(pid_t pid)
{
	int graceful_timeout = 1; /* Default value - overridden by config */
	int status=0;
	config_entry *config_timeout;
	int tmp_timeout;
	int tsl;
	int kill_status;

	if (blah_config_handle != NULL && 
	    (config_timeout=config_get("blah_graceful_kill_timeout",blah_config_handle)) != NULL)
	{
		tmp_timeout = atoi(config_timeout->value);
		if (tmp_timeout > 0) graceful_timeout = tmp_timeout;
	}

	/* verify that child is dead */
	for(tsl = 0; (waitpid(pid, &status, WNOHANG) == 0) &&
	              tsl < graceful_timeout; tsl++)
	{
		/* still alive, allow a few seconds 
		   than use brute force */
		if (tsl > (graceful_timeout/2)) 
		{
			/* Signal forked process group */
			kill(pid, SIGTERM);
		}
	}

	if (tsl >= graceful_timeout && (waitpid(pid, &status, WNOHANG) == 0))
	{
		kill_status = kill(pid, SIGKILL);

		if (kill_status == 0)
		{
			waitpid(pid, &status, 0);
		}
	}

	return(status);
}

int 
main(int argc, char *argv[])
{

	char *blah_location=NULL;
        char *parser_names[3] = {"BLParserPBS", "BLParserLSF", NULL};
	
	char *debuglevelpbs=NULL;
	char *debuglogfilepbstmp=NULL;
	char *debuglogfilepbs=NULL;
	char *spooldirpbs=NULL;
	
	char *debuglevellsf=NULL;
	char *debuglogfilelsftmp=NULL;
	char *debuglogfilelsf=NULL;
	char *binpathlsf=NULL;
	char *confpathlsf=NULL;
	
	char *portpbskey=NULL;
	char *creamportpbskey=NULL;
	char *portpbs=NULL;
	char *creamportpbs=NULL;
	
	char *portlsfkey=NULL;
	char *creamportlsfkey=NULL;
	char *portlsf=NULL;
	char *creamportlsf=NULL;
	
	char *s=NULL;
	
	char *ldir=NULL;
	
	int i;
	
		
/* Read config common part */

	if ((blah_location = getenv("BLAHPD_LOCATION")) == NULL)
	{
		blah_location = getenv("GLITE_LOCATION");
		if (blah_location == NULL) blah_location = DEFAULT_GLITE_LOCATION;
	}
	
	config_file = getenv("BLPARSER_CONFIG_LOCATION");
	if (config_file == NULL){
 		config_file = (char *)malloc(strlen(CONFIG_FILE_PARSER)+strlen(blah_location)+6);
		if(config_file == NULL){
			fprintf(stderr, "Out of memory\n");
			exit(MALLOC_ERROR);
		}
		sprintf(config_file,"%s/etc/%s",blah_location,CONFIG_FILE_PARSER);
	
		if(access(config_file,R_OK)){
			/* Last resort: default location. */
			sprintf(config_file,"/etc/%s",CONFIG_FILE_PARSER);
			
		}

	}

	if(access(config_file,R_OK)){
		fprintf(stderr, "%s does not exist or is not readable\n",config_file);
		exit(EXIT_FAILURE);
	}

        blah_config_handle = config_read(config_file);
        if (blah_config_handle == NULL)
        {
                fprintf(stderr,"Error reading config: ");
                perror("");
                return -1;
        }

	if (config_test_boolean(config_get("GLITE_CE_USE_BLPARSERPBS",blah_config_handle))){
		usepbs=1;
	}
	
	if (config_test_boolean(config_get("GLITE_CE_USE_BLPARSERLSF",blah_config_handle))){
		uselsf=1;
	}
	
	if(!usepbs && !uselsf){
		fprintf(stderr, "No parser selected (both GLITE_CE_USE_BLPARSERPBS and GLITE_CE_USE_BLPARSERLSF are not set)\n");
		exit(EXIT_FAILURE);
	}
	
	parser_pbs=malloc(MAXPARSERNUM*sizeof(struct blah_managed_child));
	parser_lsf=malloc(MAXPARSERNUM*sizeof(struct blah_managed_child));
	
	if(parser_pbs == NULL || parser_lsf == NULL){
		fprintf(stderr, "Out of memory\n");
		exit(MALLOC_ERROR);
	}
	
	/* PBS part */
	
	if(usepbs){

        	ret = config_get("GLITE_CE_BLPARSERPBS_DEBUGLEVEL",blah_config_handle);
        	if (ret != NULL){
			debuglevelpbs = make_message("-d %s",ret->value);
			if(debuglevelpbs == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERPBS_DEBUGLOGFILE",blah_config_handle);
        	if (ret != NULL){
			debuglogfilepbstmp = make_message("-l %s",ret->value);
			if(debuglogfilepbstmp == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERPBS_SPOOLDIR",blah_config_handle);
        	if (ret != NULL){
			ldir=make_message("%s/server_logs",ret->value);
			if (opendir(ldir)==NULL){
				fprintf(stderr, "dir %s does not exist or is not readable\n",ldir);
				exit(EXIT_FAILURE);
			}
			free(ldir);
			spooldirpbs = make_message("-s %s",ret->value);
			if(spooldirpbs == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}else{
			fprintf(stderr, "GLITE_CE_BLPARSERPBS_SPOOLDIR is not defined\n");
			exit(EXIT_FAILURE);
		}

        	ret = config_get("GLITE_CE_BLPARSERPBS_NUM",blah_config_handle);
        	if (ret != NULL){
        	        parsernumpbs=atoi(ret->value);
        	}else{
			parsernumpbs=1;
		}
	
		for(i=0;i<parsernumpbs;i++){
			portpbskey = make_message("GLITE_CE_BLPARSERPBS_PORT%d",i+1);
			creamportpbskey = make_message("GLITE_CE_BLPARSERPBS_CREAMPORT%d",i+1);
			if(portpbskey == NULL || creamportpbskey == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
			ret = config_get(portpbskey,blah_config_handle);
			if (ret != NULL){
				portpbs = make_message("-p %s",ret->value);
				if(portpbs == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			ret = config_get(creamportpbskey,blah_config_handle);
			if (ret != NULL){
				creamportpbs = make_message("-m %s",ret->value);
				if(creamportpbs == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			if(parsernumpbs>1){
				debuglogfilepbs=make_message("%s-%d",debuglogfilepbstmp,i+1);
				if(debuglogfilepbs == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}else{
				debuglogfilepbs=make_message("%s",debuglogfilepbstmp);
				if(debuglogfilepbs == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			
			s=make_message("%s/libexec/%s",blah_location,parser_names[0]);                
			if(access(s,X_OK)){
				fprintf(stderr, "%s does not exist or it is not executable\n",s);
				exit(EXIT_FAILURE);
			}
			free(s);
			
			parser_pbs[i].exefile = make_message("%s/libexec/%s %s %s %s %s %s",blah_location,parser_names[0],debuglevelpbs,debuglogfilepbs,spooldirpbs,portpbs,creamportpbs);
			parser_pbs[i].pidfile = make_message("%s/%s%d.pid",PID_DIR,parser_names[0],i+1);

			if(parser_pbs[i].exefile == NULL || parser_pbs[i].pidfile == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
			
			free(portpbskey);
			free(creamportpbskey);
			free(portpbs);
			free(creamportpbs);
		}
		free(debuglevelpbs);
		free(debuglogfilepbstmp);
		free(debuglogfilepbs);
		free(spooldirpbs);
	}
	
	/* LSF part */
	
	if(uselsf){

        	ret = config_get("GLITE_CE_BLPARSERLSF_DEBUGLEVEL",blah_config_handle);
        	if (ret != NULL){
			debuglevellsf = make_message("-d %s",ret->value);
			if(debuglevellsf == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERLSF_DEBUGLOGFILE",blah_config_handle);
        	if (ret != NULL){
			debuglogfilelsftmp = make_message("-l %s",ret->value);
			if(debuglogfilelsftmp == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERLSF_BINPATH",blah_config_handle);
        	if (ret != NULL){
			binpathlsf = make_message("-b %s",ret->value);
			if(binpathlsf == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERLSF_CONFPATH",blah_config_handle);
        	if (ret != NULL){
			confpathlsf = make_message("-c %s",ret->value);
			if(confpathlsf == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
        	}

        	ret = config_get("GLITE_CE_BLPARSERLSF_NUM",blah_config_handle);
        	if (ret != NULL){
        	        parsernumlsf=atoi(ret->value);
        	}else{
			parsernumlsf=1;
		}

		for(i=0;i<parsernumlsf;i++){
			portlsfkey = make_message("GLITE_CE_BLPARSERLSF_PORT%d",i+1);
			creamportlsfkey = make_message("GLITE_CE_BLPARSERLSF_CREAMPORT%d",i+1);
			if(portlsfkey == NULL || creamportlsfkey == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
			ret = config_get(portlsfkey,blah_config_handle);
			if (ret != NULL){
				portlsf = make_message("-p %s",ret->value);
				if(portlsf == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			ret = config_get(creamportlsfkey,blah_config_handle);
			if (ret != NULL){
				creamportlsf = make_message("-m %s",ret->value);
				if(creamportlsf == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			if(parsernumlsf>1){
				debuglogfilelsf=make_message("%s-%d",debuglogfilelsftmp,i+1);
				if(debuglogfilelsf == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}else{
				debuglogfilelsf=make_message("%s",debuglogfilelsftmp);
				if(debuglogfilelsf == NULL){
					fprintf(stderr, "Out of memory\n");
					exit(MALLOC_ERROR);
				}
			}
			
			s=make_message("%s/libexec/%s",blah_location,parser_names[1]);                
			if(access(s,X_OK)){
				fprintf(stderr, "%s does not exist or it is not executable\n",s);
				exit(EXIT_FAILURE);
			}
			
			free(s);
			parser_lsf[i].exefile = make_message("%s/libexec/%s %s %s %s %s %s %s",blah_location,parser_names[1],debuglevellsf,debuglogfilelsf,binpathlsf,confpathlsf,portlsf,creamportlsf);
			parser_lsf[i].pidfile = make_message("%s/%s%d.pid",PID_DIR,parser_names[1],i+1);
			
			if(parser_lsf[i].exefile == NULL || parser_lsf[i].pidfile == NULL){
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}

			free(portlsfkey);
			free(creamportlsfkey);
			free(portlsf);
			free(creamportlsf);
		}
		free(debuglevellsf);
		free(debuglogfilelsftmp);
		free(debuglogfilelsf);
		free(binpathlsf);
		free(confpathlsf);
	}
	
	/* signal handler */
	
	signal(SIGTERM,sigterm);
	
	daemonize();
		
	while(1){
	
		
		if (usepbs && parsernumpbs>0) check_on_children_args(parser_pbs, parsernumpbs);
		if (uselsf && parsernumlsf>0) check_on_children_args(parser_lsf, parsernumlsf);
		sleep(2);
	}
		
	exit(EXIT_SUCCESS);
}
