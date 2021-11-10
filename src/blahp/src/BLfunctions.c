/*
#  File:     BLfunctions.c
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

#include "BLfunctions.h"

int dmn=0;
int debug=0;

ssize_t
Readline(int sockd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char    c, *buffer;

	buffer = vptr;

	for ( n = 1; n < maxlen; n++ ) {
	
		if ( (rc = read(sockd, &c, 1)) == 1 ) {
			*buffer++ = c;
			if ( c == '\n' ) {
				break;
			}
		} else if ( rc == 0 ) {
			if ( n == 1 ) {
				return 0;
			} else {
				break;
			}
		} else {
			if ( errno == EINTR ) {
				continue;
			}
			return -1;
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

int
GetRdxId(int cnt)
{
	int i;

	if(cnt == 0){
		return -1;
	}

	for(i=0;i<RDXHASHSIZE;i++){
		if(rptr[i] == cnt){
			return i;
		}
	}
	return -1;
}

int
GetBlahNameId(char *blahstr)
{
	
	int i;
	
	if(blahstr == NULL){
		return -1;
	}
	
	for(i=0;i<RDXHASHSIZE;i++){
		if(j2bl[i]!=NULL && strcmp(j2bl[i],blahstr)==0){
			return i;
		}
	}
	return -1;

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

int
str2epoch(char *str, char * f)
{

        char *dateout;
        int idate;

        struct tm tm;

        if(strcmp(f,"S")==0){
                strptime(str,"%Y-%m-%d %H:%M:%S",&tm);
        }else if(strcmp(f,"L")==0){
                strptime(str,"%a %b %d %H:%M:%S %Y",&tm);
        }else if(strcmp(f,"D")==0){
                strptime(str,"%Y%m%d",&tm);
        }

        if((dateout=calloc(NUM_CHARS,1)) == 0){
                sysfatal("can't malloc dateout in str2epoch: %r");
        }

        strftime(dateout,NUM_CHARS,"%s",&tm);

        idate=atoi(dateout);
        free(dateout);

        return idate;

}

char *
iepoch2str(time_t epoch, char * f)
{
  
	char *dateout;
	char *lepoch;

	struct tm tm;
	
	lepoch=make_message("%d",epoch);
 
	strptime(lepoch,"%s",&tm);
 
	if((dateout=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc dateout in iepoch2str: %r");
	}
 
	if(strcmp(f,"S")==0){
		strftime(dateout,NUM_CHARS,"%Y%m%d",&tm);
	}else if(strcmp(f,"L")==0){
		strftime(dateout,NUM_CHARS,"%Y%m%d%H%M.%S",&tm);
	}else if(strcmp(f,"D")==0){
	        strftime(dateout,NUM_CHARS,"%Y-%m-%d %H:%M:%S",&tm);
	}

	free(lepoch);
 
	return dateout;
 
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


long GetHistorySeekPos(FILE *fp){

	char *cp;
	char *tline;
	char *tlineout;
	long lret=0;
	
	if((tline=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc tline in GetHistorySeekPos: %r");
	}

	
	if(fseek(fp, 0L, SEEK_SET) < 0){
		syserror("poll() timeout in NotifyCream: %r");
		return 0L;
	}
        while(fgets(tline, NUM_CHARS, fp)){
		if ((cp = strrchr (tline, '\n')) != NULL){
			*cp = '\0';
			tlineout=strdel(tline, "#");
			lret=atol(tlineout);
			free(tline);
			free(tlineout);
			return lret;
       		}
	}

	return 0L;	
}


int 
do_log(FILE *debuglogfile, int debuglevel, int dbgthresh, const char *fmt, ...)
{

        va_list ap;
        char *dbgtimestamp; 

        if(debuglevel == 0){
                return 0;
        }

        if(debuglevel>=dbgthresh){
                dbgtimestamp=iepoch2str(time(0),"D");
                fprintf(debuglogfile,"%s ",dbgtimestamp);
                free(dbgtimestamp);
                va_start(ap, fmt);
                vfprintf(debuglogfile,fmt,ap);
                fflush(debuglogfile);
                va_end(ap);
        }

        return 0;       
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
