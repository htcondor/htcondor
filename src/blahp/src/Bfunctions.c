/*
#  File:     Bfunctions.c
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

#include "Bfunctions.h"

pthread_mutex_t writeline_mutex = PTHREAD_MUTEX_INITIALIZER;
int bfunctions_poll_timeout = 600000; /* Default 10 minutes */

ssize_t
Readline(int sockd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char    c, *buffer;
	struct   pollfd fds[2];
	struct   pollfd *pfds;
	int      nfds = 1;
	int      retcod;
    
	fds[0].fd = sockd;
	fds[0].events = 0;
	fds[0].events = ( POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL ) ;
	pfds = fds;

	buffer = vptr;
	
	for ( n = 1; n < maxlen; n++ ) {
	
		retcod = poll(pfds, nfds, -1);
		if( retcod < 0 ){
			syserror("Poll error in Readline: %r");
			break;
		} else if( retcod == 0 ){
			syserror("Poll timeout in Readline: %r");
			break;
		} else if ( retcod > 0 ){		
			if ( ( fds[0].revents & ( POLLERR | POLLNVAL | POLLHUP) )){
				switch (fds[0].revents){
				case POLLNVAL:
					syserror("poll() file descriptor error in Readline: %r");
					return -1;
				case POLLHUP:
					syserror("Connection closed in Readline: %r");
					return -1;
				case POLLERR:
					syserror("poll() POLLERR in Readline: %r");
					return -1;
				}
			} else {

				if ( (rc = read(sockd, &c, 1)) == 1 ) {
					*buffer++ = c;
					if ( c == '\n' ){
						break;
					}
				} else if ( rc == 0 ) {
					return 0;
				} else {
					if ( errno == EINTR ){
						continue;
					}
					return -1;
				}
			}
		}
	}

	*buffer = 0;
	return n;
}

ssize_t
Writeline(int sockd, const void *vptr, size_t n)
{
	size_t      nleft;
	ssize_t     nwritten;
	const char *buffer;

	buffer = vptr;
	nleft  = n;

	/* set write lock */
	pthread_mutex_lock( &writeline_mutex );

	while ( nleft > 0 ) {

		if ( (nwritten = write(sockd, (char *)vptr, nleft)) <= 0 ) {
			if ( errno == EINTR ) {
				nwritten = 0;
			}else{
				return -1;
			}
		}
		nleft  -= nwritten;
		buffer += nwritten;
	}

	/* release write lock */
	pthread_mutex_unlock( &writeline_mutex );

	return n;
}

char *get_line(FILE * f)
{
    size_t size = 0;
    size_t len  = 0;
    size_t last = 0;
    char * buf  = NULL;
    struct pollfd pfd;
    int rpoll;


    do {
        pfd.fd = fileno(f);
        pfd.events = ( POLLIN | POLLERR | POLLHUP | POLLNVAL );
        pfd.revents = 0;
        rpoll = poll(&pfd, 1, bfunctions_poll_timeout);
        if (rpoll <= 0 || ((pfd.revents & (POLLIN|POLLHUP)) == 0 ) ||
            feof(f) ) break;
        size += BUFSIZ;
        buf = realloc(buf,size);           
        buf[last]='\000';
	if (fgets(buf+last,size-last,f) == NULL) break;
        len = strlen(buf);
        last = len - 1;
    } while (!feof(f) && buf[last]!='\n');
    return buf;
}

int
freetoken(char ***token, int maxtok)
{
	int i;
	
	for(i=0;i<maxtok;i++){
		free((*token)[i]);
	}
	free(*token);
	
	return 0;
}

int
strtoken(const char *s, char delim, char ***token)
{
        char *tmp;
        char *ptr, *dptr;
	char **t=NULL;
        int i = 0;
	int numtok=0;
	
	
        if((tmp = calloc(1 + strlen(s),1)) == 0){
                sysfatal("can't malloc tmp: %r");
        }
        assert(tmp);
        strcpy(tmp, s);
        ptr = tmp;
	
	*token=NULL;
	
        while(1) {
	
		if(i >=numtok-1){
			numtok+=NUMTOK;
			t=realloc(*token,numtok*sizeof(char *));
			if(t != NULL){
				*token=t;
			}else{
				sysfatal("can't realloc token: %r");
			}
		}
				
                if((dptr = strchr(ptr, delim)) != NULL) {
                        *dptr = '\0';
                        if(((*token)[i] = calloc(1 + strlen(ptr),1)) == 0){
                                sysfatal("can't malloc (*token)[i]: %r");
                        }
                        assert((*token)[i]);
                        strcpy((*token)[i], ptr);
                        ptr = dptr + 1;
                        if (strlen((*token)[i]) != 0){
                                i++;
                        }else{
				free((*token)[i]);
			}
                } else {
                        if(strlen(ptr)) {
                                if(((*token)[i] = calloc(1 + strlen(ptr),1)) == 0){
                                        sysfatal("can't malloc (*token)[i]: %r");
                                }
                                assert((*token)[i]);
                                strcpy((*token)[i], ptr);
                                i++;
                                break;
                        } else{
                                break;
                        }
                }
        }

        (*token)[i] = NULL;
        free(tmp);
        return i;

}

char *
strdel(char *s, const char *delete)
{
	char *tmp, *cptr, *sptr;
    
	if(!delete || !strlen(delete)){
		tmp = strndup(s, STR_CHARS);
		return tmp;
	}
        
	if(!s || !strlen(s)){
		tmp = strndup(s, STR_CHARS);
		return tmp;
	}
        
	tmp = strndup(s, STR_CHARS);
       
	assert(tmp);
    
	for(sptr = tmp; (cptr = strpbrk(sptr, delete)); sptr = tmp) {
		*cptr = '\0';
		strcat(tmp, ++cptr);
	}
    
	return tmp;
}

char *
epoch2str(char *epoch)
{
  
	char *dateout;

	struct tm tm;

	strptime(epoch,"%s",&tm);
 
	if((dateout=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc dateout in epoch2str: %r");
	}
 
	strftime(dateout,NUM_CHARS,"%Y-%m-%d %H:%M:%S",&tm);
 
	return dateout;
 
}

char *
iepoch2str(time_t epoch)
{
  
	char *dateout;
	char *lepoch;

	struct tm tm;
	
	lepoch=make_message("%d",epoch);
 
	strptime(lepoch,"%s",&tm);
 
	if((dateout=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc dateout in iepoch2str: %r");
	}
 
        strftime(dateout,NUM_CHARS,"%Y-%m-%d %H:%M:%S",&tm);
	free(lepoch);
 
	return dateout;
 
}

time_t
str2epoch(char *str, char * f)
{
  
	char *strtmp;
	time_t idate;
	time_t now;

	struct tm tm;
        struct tm tmnow;
	
	int mdlog,mdnow;
	
	if(strcmp(f,"S")==0){
		strptime(str,"%Y-%m-%d %T",&tm);
	}else if(strcmp(f,"L")==0){
		strptime(str,"%a %b %d %T %Y",&tm);
        }else if(strcmp(f,"A")==0){
                strptime(str,"%m/%d/%Y %T",&tm);
	}else if(strcmp(f,"W")==0){
		
	/* If do not have the year in the date we compare day and month and set the year */
		
		strtmp=make_message("%s 2000",str);
                strptime(strtmp,"%a %b %d %T %Y",&tm);
		
		now=time(0);
		localtime_r(&now,&tmnow);
		
		mdlog=(tm.tm_mon)*100+tm.tm_mday;
		mdnow=(tmnow.tm_mon)*100+tmnow.tm_mday;
		if(mdlog > mdnow){
			tm.tm_year=tmnow.tm_year-1;
		}else{
			tm.tm_year=tmnow.tm_year;
		}
		
	        free(strtmp);
        }else if(strcmp(f,"V")==0){

        /* If do not have the year in the date we compare day and month and set the year */

		strtmp=make_message("%s 2000",str);
                strptime(strtmp,"%b %d %H:%M %Y",&tm);
                
                now=time(0);
		localtime_r(&now,&tmnow);

                mdlog=(tm.tm_mon)*100+tm.tm_mday;
                mdnow=(tmnow.tm_mon)*100+tmnow.tm_mday;
                if(mdlog > mdnow){
                        tm.tm_year=tmnow.tm_year-1;
                }else{
                        tm.tm_year=tmnow.tm_year;
                }

                free(strtmp);

        }
 
	tm.tm_isdst=-1;
	idate=mktime(&tm);
 
	return idate;
 
}

void
daemonize()
{

	int pid;
    
	pid = fork();
	
	if (pid < 0){
		sysfatal("Cannot fork in daemonize: %r");
	}else if (pid >0){
		exit(EXIT_SUCCESS);
	}
    
	setsid();
    
	pid = fork();
	
	if (pid < 0){
		sysfatal("Cannot fork in daemonize: %r");
	}else if (pid >0){
		exit(EXIT_SUCCESS);
	}
	chdir("/");
    
	freopen ("/dev/null", "r", stdin);  
	freopen ("/dev/null", "w", stdout);
	freopen ("/dev/null", "w", stderr); 

}

int
writepid(char * pidfile)
{
	FILE *fpid;
	struct stat pstat;
	mode_t pidmode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

	if (stat(pidfile, &pstat) >= 0){
		pidmode = pstat.st_mode;
		pidmode &= ~(S_IWGRP|S_IWOTH);
	}

	fpid = fopen(pidfile, "w");
	if ( !fpid ) { perror(pidfile); return 1; }
	fchmod(fileno(fpid), pidmode);

	if (fprintf(fpid, "%d", getpid()) <= 0) { perror(pidfile); return 1; }
	if (fclose(fpid) != 0) { perror(pidfile); return 1; }
	return 0;
}

void
eprint(int err, char *fmt, va_list args)
{
	extern int errno;

	fprintf(stderr, "%s: ", argv0);
	if(fmt){
		vfprintf(stderr, fmt, args);
	}
	if(err){
		fprintf(stderr, "%s", strerror(errno));
	}
	fputs("\n", stderr);
	errno = 0;
}

char *
chopfmt(char *fmt)
{
	static char errstr[ERRMAX];
	char *p;

	errstr[0] = '\0';
	if((p=strstr(fmt, "%r")) != 0){
		fmt = strncat(errstr, fmt, p-fmt);
	}
	return fmt;
}

/* syserror: print error and continue */
void
syserror(char *fmt, ...)
{
	va_list args;
	char *xfmt;

	va_start(args, fmt);
	xfmt = chopfmt(fmt);
	eprint(xfmt!=fmt, xfmt, args);
	va_end(args);
}

/* sysfatal: print error and die */
void
sysfatal(char *fmt, ...)
{
	va_list args;
	char *xfmt;

	va_start(args, fmt);
	xfmt = chopfmt(fmt);
	eprint(xfmt!=fmt, xfmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

int
bupdater_push_active_job(bupdater_active_jobs *bact, const char *job_id)
{
  char **new_jobs;

  new_jobs = (char **) realloc(bact->jobs, (bact->njobs+1) * sizeof(char *));
  if (new_jobs == NULL) return BUPDATER_ACTIVE_JOBS_FAILURE;

  bact->jobs = new_jobs;
  bact->jobs[bact->njobs] = strdup(job_id);
  if (bact->jobs[bact->njobs] == NULL) return BUPDATER_ACTIVE_JOBS_FAILURE;
  bact->njobs++;

  bact->is_sorted = 0;

  return BUPDATER_ACTIVE_JOBS_SUCCESS;
}

void
bupdater_sort_active_jobs(bupdater_active_jobs *bact, int left, int right)
{
  int psize;
  int mother = 0;
  char *max, *median, *min, *pick, *swap;
  int i,k;

  if (left == 0 && right == bact->njobs-1) mother = 1;
  if (mother != 0)
   {
    if (bact->is_sorted != 0) return;
    if (bact->njobs < 2) return;
   }

  /* Singly-recursive quicksort of job entries  */

  while (left < right)
   {
    psize = right - left + 1;

    /* Shortcut for trivial case. */
    if (psize == 2 && (strcmp(bact->jobs[left], bact->jobs[right]) < 0)) return;

    /* Choose a partition value with the "median-of-three" method. */
    max = min = bact->jobs[left + rand()%psize];

    pick = bact->jobs[left + rand()%psize];
    if (strcmp(pick, max) > 0) max = pick;
    else if (strcmp (pick, min) < 0) min = pick;

    pick = bact->jobs[left + rand()%psize];
    if (strcmp(pick, max) > 0) median = max;
    else if (strcmp(pick, min) < 0) median = min;
    else median = pick;

    for (i = left, k = right; ; i++,k--)
     {
      while (strcmp(bact->jobs[i], median) < 0) i++;
      while (strcmp(bact->jobs[k], median) > 0) k--;

      /* Now stop if indexes crossed. This way we are sure that k is the
         last element of the left partition. */
      if (i>=k) break;

      /* We found a pair that's out of order. Let's swap them. */
      swap = bact->jobs[i];
      bact->jobs[i] = bact->jobs[k];
      bact->jobs[k] = swap;
     }

    /* Operate on the left and right sub-partitions. */
    bupdater_sort_active_jobs(bact,left,k);

    /* Do the right partition on the next while loop iteration */
    left = k+1;
   }

  if (mother != 0)
   {
    bact->is_sorted = 1;
   }
  return;
}

int
bupdater_lookup_active_jobs(bupdater_active_jobs *bact, 
                            const char *job_id)
{
  int left, right, cur, cmp;
  if (bact->is_sorted == 0) bupdater_sort_active_jobs(bact,0,bact->njobs-1);

  /* Binary search of needed entry */
  left = 0; right = bact->njobs-1;

  while (right >= left)
   {
    cur = (right + left) /2;
    cmp = strcmp(bact->jobs[cur],job_id);
    if (cmp == 0)
     {
      return BUPDATER_ACTIVE_JOBS_SUCCESS;
     }
    else if (cmp < 0)
     {
      left = cur+1;
     }
    else
     {
      right = cur-1;
     }
   }

  return BUPDATER_ACTIVE_JOBS_FAILURE;
}

int
bupdater_remove_active_job(bupdater_active_jobs *bact, 
                           const char *job_id)
{
  int left, right, cur, cmp, resize;
  if (bact->is_sorted == 0) bupdater_sort_active_jobs(bact,0,bact->njobs-1);

  /* Binary search of needed entry */
  left = 0; right = bact->njobs-1;

  while (right >= left)
   {
    cur = (right + left) /2;
    cmp = strcmp(bact->jobs[cur],job_id);
    if (cmp == 0)
     {
      /* Job ID found. Remove it from list. */
      free(bact->jobs[cur]);
      for (resize = cur+1; resize<bact->njobs; resize++)
       {
        bact->jobs[resize - 1] = bact->jobs[resize];
       }
      bact->jobs[bact->njobs-1] = "";
      (bact->njobs)--;
      return BUPDATER_ACTIVE_JOBS_SUCCESS;
     }
    else if (cmp < 0)
     {
      left = cur+1;
     }
    else
     {
      right = cur-1;
     }
   }

  return BUPDATER_ACTIVE_JOBS_FAILURE;
}

void
bupdater_free_active_jobs(bupdater_active_jobs *bact)
{
  int i;
  if (bact->jobs == NULL) return;

  for (i=0; i<bact->njobs; i++)
    if (bact->jobs[i] != NULL) free(bact->jobs[i]);

  free(bact->jobs); 
  bact->jobs = NULL;
  bact->njobs = 0;
}

int do_log(FILE *debuglogfile, int debuglevel, int dbgthresh, const char *fmt, ...){

        va_list ap;
	char *dbgtimestamp;
	
	if(debuglevel == 0){
		return 0;
	}
	
        if(debuglevel>=dbgthresh){
		dbgtimestamp=iepoch2str(time(0));
		fprintf(debuglogfile,"%s ",dbgtimestamp);
		free(dbgtimestamp);
		va_start(ap, fmt);
		vfprintf(debuglogfile,fmt,ap);
		fflush(debuglogfile);
        	va_end(ap);
	}
	
	return 0;	
} 

int check_config_file(char *logdev){
	
	char *s=NULL;
        struct stat sbuf;
        int rc;
	config_handle *lcha;
	config_entry *lret;
	char *supplrms=NULL;
	char *reg_file=NULL;
	char *pbs_path=NULL;
	char *lsf_path=NULL;
	char *condor_path=NULL;
	char *pbs_spool=NULL;
	char *sge_root=NULL;
	char *sge_cell=NULL;
	char *sge_helperpath=NULL;
	char *sge_path=NULL;
	char *ldebuglogname=NULL;
	FILE *ldebuglogfile;
	int  ldebug;
	int async_port;
		
        lcha = config_read(NULL);
        if (lcha == NULL) {
		sysfatal("Error reading config: %r");
        }

/* Get debug level and debug log file info to log possible problems */

	if(strcmp(logdev,"STDOUT")==0){
		ldebug=0;
	}else if(strcmp(logdev,"UPDATER")==0){
		lret = config_get("bupdater_debug_level",lcha);
		if (lret != NULL){
			ldebug=atoi(lret->value);
		}
	
		lret = config_get("bupdater_debug_logfile",lcha);
		if (lret != NULL){
			ldebuglogname=strdup(lret->value);
			if(ldebuglogname == NULL){
                        	sysfatal("strdup failed for ldebuglogname in check_config_file: %r");
			}
		}
	}else if(strcmp(logdev,"NOTIFIER")==0){
		lret = config_get("bnotifier_debug_level",lcha);
		if (lret != NULL){
			ldebug=atoi(lret->value);
		}
	
		lret = config_get("bnotifier_debug_logfile",lcha);
		if (lret != NULL){
			ldebuglogname=strdup(lret->value);
			if(ldebuglogname == NULL){
                        	sysfatal("strdup failed for ldebuglogname in check_config_file: %r");
			}
		}
	}
	
	if(ldebug <=0){
		ldebug=0;
	}
    
	if(ldebuglogname){
		if((ldebuglogfile = fopen(ldebuglogname, "a+"))==0){
			ldebug = 0;
		}
	}else{
		ldebug = 0;
	}
	
/* Get supported_lrms key */

        lret = config_get("supported_lrms",lcha);
	if (lret == NULL){
		do_log(ldebuglogfile, ldebug, 1, "%s: key supported_lrms not found\n",argv0);
		sysfatal("supported_lrms not defined. Exiting");
        }else{
                supplrms=strdup(lret->value);
                if(supplrms == NULL){
                        sysfatal("strdup failed for supplrms in check_config_file: %r");
                }
        }
	
/* Check if job_registry is defined */
      
	lret = config_get("job_registry",lcha);
	if (lret == NULL){
		do_log(ldebuglogfile, ldebug, 1, "%s: key job_registry not found\n",argv0);
		sysfatal("job_registry not defined. Exiting");
        }else{
                reg_file=strdup(lret->value);
                if(reg_file == NULL){
                        sysfatal("strdup failed for reg_file in check_config_file: %r");
                }
        }
	free(reg_file);
	
/* Check if async_notification_port is defined */
      
        lret = config_get("async_notification_port",lcha);
	if (lret == NULL){
		do_log(ldebuglogfile, ldebug, 1, "%s: key async_notification_port not found\n",argv0);
		sysfatal("async_notification_port not defined. Exiting");
	} else {
                async_port=atoi(lret->value);
        }

	if(strstr(supplrms,"pbs")){
	
/* Check if pbs_binpath exists and that the programs that use it are executables */

        	lret = config_get("pbs_binpath",lcha);
		if (lret == NULL){
			do_log(ldebuglogfile, ldebug, 1, "%s: key pbs_binpath not found\n",argv0);
			sysfatal("pbs_binpath not defined. Exiting");
		} else {
			pbs_path=strdup(lret->value);
                	if(pbs_path == NULL){
                        	sysfatal("strdup failed for pbs_path in check_config_file: %r");
                	}
        	}
		
		s=make_message("%s/qstat",pbs_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,pbs_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",pbs_path,s);
		}
		free(s);
		s=make_message("%s/tracejob",pbs_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,pbs_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",pbs_path,s);
		}
		free(s);
	
		free(pbs_path);
		
/* Check if pbs_spoolpath/server_logs exists and is accessible */

		lret = config_get("pbs_spoolpath",lcha);
		if (lret == NULL){
			do_log(ldebuglogfile, ldebug, 1, "%s: key pbs_spoolpath not found\n",argv0);
			sysfatal("pbs_spoolpath not defined. Exiting");
		} else {
			pbs_spool=strdup(lret->value);
                	if(pbs_spool == NULL){
                        	sysfatal("strdup failed for pbs_spool in check_config_file: %r");
                	}
		}
		
		s=make_message("%s/server_logs",pbs_spool);
		rc=stat(s,&sbuf);
		if(rc) {
			do_log(ldebuglogfile, ldebug, 1, "%s: dir %s does not exist\n",argv0,s);
			sysfatal("dir %s does not exist: %r",s);
		}
		if(! S_ISDIR(sbuf.st_mode)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not a dir\n",argv0,s);
			sysfatal("%s is not a dir: %r",s);
		}	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: dir %s is not accessible\n",argv0,s);
			sysfatal("dir %s is not accessible: %r",s);
		}	
		free(s);	
		
		free(pbs_spool);		
		
	}
	if(strstr(supplrms,"lsf")){
	
/* Check if lsf_binpath exists  and that the programs that use it are executables */

        	lret = config_get("lsf_binpath",lcha);
		if (lret == NULL){
			do_log(ldebuglogfile, ldebug, 1, "%s: key lsf_binpath not found\n",argv0);
			sysfatal("lsf_binpath not defined. Exiting");
		} else {
			lsf_path=strdup(lret->value);
                	if(lsf_path == NULL){
                        	sysfatal("strdup failed for lsf_path in check_config_file: %r");
                	}
		}
		
		s=make_message("%s/bjobs",lsf_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,lsf_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",lsf_path,s);
		}
		free(s);
		s=make_message("%s/bhist",lsf_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,lsf_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",lsf_path,s);
		}
		free(s);
		
		free(lsf_path);		
		
	}
	if(strstr(supplrms,"condor")){
	
/* Check if condor_binpath exists  and that the programs that use it are executables */

		lret = config_get("condor_binpath",lcha);
		if (lret == NULL){
			do_log(ldebuglogfile, ldebug, 1, "%s: key condor_binpath not found\n",argv0);
			sysfatal("condor_binpath not defined. Exiting");
		} else {
			condor_path=strdup(lret->value);
                	if(condor_path == NULL){
                        	sysfatal("strdup failed for condor_path in check_config_file: %r");
                	}
		}
		
		s=make_message("%s/condor_q",condor_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,condor_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",condor_path,s);
		}
		free(s);
		s=make_message("%s/condor_history",condor_path);	
		if(access(s,X_OK)){
			do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,condor_path,s);
			sysfatal("%s is not accessible or %s is not executable: %r",condor_path,s);
		}
		free(s);
		
		free(condor_path);
	
	}
        if(strstr(supplrms,"sge")){

/* Check if sge_root,sge_cell and sge_helper exists  and that the programs that use it are executables */

                lret = config_get("sge_binpath",lcha);
                if (lret == NULL){
                        do_log(ldebuglogfile, ldebug, 1, "%s: key sge_binpath not found\n",argv0);
                        sysfatal("sge_binpath not defined. Exiting");
                } else {
                        sge_path=strdup(lret->value);
                        if(sge_path == NULL){
                                sysfatal("strdup failed for sge_path in check_config_file: %r");
                        }
                }

	        s=make_message("%s/qstat",sge_path);
                if(access(s,X_OK)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,sge_path,s);
                        sysfatal("%s is not accessible or %s is not executable: %r",sge_path,s);
                }
                free(s);
                s=make_message("%s/qacct",sge_path);
                if(access(s,X_OK)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: %s is not accessible or %s is not executable\n",argv0,sge_path,s);
                        sysfatal("%s is not accessible or %s is not executable: %r",sge_path,s);
                }
                free(s);

                free(sge_path);

/* Check if sge_cellname exists and is accessible */

                lret = config_get("sge_cellname",lcha);
                if (lret == NULL){
                        do_log(ldebuglogfile, ldebug, 1, "%s: key sge_cellname not found\n",argv0);
                        sysfatal("sge_cellname not defined. Exiting");
                } else {
                        sge_cell=strdup(lret->value);
                        if(sge_cell == NULL){
                                sysfatal("strdup failed for sge_cell in check_config_file: %r");
                        }
                }

                free(sge_cell);

/* Check if sge_rootpath exists and is accessible */

                lret = config_get("sge_rootpath",lcha);
                if (lret == NULL){
                        do_log(ldebuglogfile, ldebug, 1, "%s: key sge_rootpath not found\n",argv0);
                        sysfatal("sge_rootpath not defined. Exiting");
                } else {
                        sge_root=strdup(lret->value);
                        if(sge_root == NULL){
                                sysfatal("strdup failed for sge_root in check_config_file: %r");
                        }
                }

                s=make_message("%s",sge_root);
                rc=stat(s,&sbuf);
                if(rc) {
                        do_log(ldebuglogfile, ldebug, 1, "%s: dir %s does not exist\n",argv0,s);
                        sysfatal("dir %s does not exist: %r",s);
                }
                if(! S_ISDIR(sbuf.st_mode)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: %s is not a dir\n",argv0,s);
                        sysfatal("%s is not a dir: %r",s);
                }
                if(access(s,X_OK)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: dir %s is not accessible\n",argv0,s);
                        sysfatal("dir %s is not accessible: %r",s);
                }
                free(s);

                free(sge_root);
		/*rc=stat(s,&sbuf);
                if(rc) {
                        do_log(ldebuglogfile, ldebug, 1, "%s: dir %s does not exist\n",argv0,s);
                        sysfatal("dir %s does not exist: %r",s);
                }
                if(! S_ISDIR(sbuf.st_mode)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: %s is not a dir\n",argv0,s);
                        sysfatal("%s is not a dir: %r",s);
                }
                if(access(s,X_OK)){
                        do_log(ldebuglogfile, ldebug, 1, "%s: dir %s is not accessible\n",argv0,s);
                        sysfatal("dir %s is not accessible: %r",s);
                }*/
        }
	
	free(supplrms);
	free(ldebuglogname);
	if(ldebug!=0){
		fclose(ldebuglogfile);
	}

	return 0;
}

char *GetPBSSpoolPath(char *binpath)
{
	char *command_string=NULL;
	int len=0;
	FILE *file_output;
	char *pbs_spool=NULL;

	command_string=make_message("%s/tracejob | grep 'default prefix path'|awk -F\" \" '{ print $5 }'",binpath);
        file_output = popen(command_string,"r");
	
        if((pbs_spool=calloc(STR_CHARS,1)) == 0){
                sysfatal("can't malloc pbs_spool in GetPBSSpoolpath: %r");
        }

        if (file_output != NULL){
                len = fread(pbs_spool, sizeof(char), STR_CHARS - 1 , file_output);
                if (len>0){
                        pbs_spool[len-1]='\000';
                }
        }
        pclose(file_output);
	free(command_string);
	
	return pbs_spool;

}
