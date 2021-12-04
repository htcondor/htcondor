/*
#  File:     BLParserPBS.c
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

#include "BLParserPBS.h"

int 
main(int argc, char *argv[])
{

	int       i,j;
	int       set = 1;
	int       status;
	int       list_s;
	int       list_c;
	char 	  ainfo_port_string[16];
	struct    addrinfo ai_req, *ai_ans, *cur_ans;
	int       address_found;
	int       c;
	
	static int help;
	static int short_help;
    
	char      *eventsfile;
    
	time_t  now;
	struct tm *tptr;
   
	int version=0;

	pthread_t ReadThd[NUMTHRDS];
	pthread_t UpdateThd;
	pthread_t CreamThd;
   
	char *tspooldir;

	argv0 = argv[0];

	/*Ignore sigpipe*/
    
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP,sighup);
        
	while (1) {
		static struct option long_options[] =
		{
		{"help",      no_argument,     &help,       1},
		{"usage",     no_argument,     &short_help, 1},
		{"daemon",    no_argument,             0, 'D'},
		{"version",   no_argument,             0, 'v'},
		{"port",      required_argument,       0, 'p'},
		{"creamport", required_argument,       0, 'm'},
		{"binpath",   required_argument,       0, 'b'},
		{"spooldir",  required_argument,       0, 's'},
		{"logfile",   required_argument,       0, 'l'},
		{"debug",     required_argument,       0, 'd'},
		{0, 0, 0, 0}
		};

		int option_index = 0;
     
		c = getopt_long (argc, argv, "Dvp:m:s:l:d:",long_options, &option_index);
     
		if (c == -1){
			break;
		}
     
		switch (c)
		{

		case 0:
		if (long_options[option_index].flag != 0){
			break;
		}
     
		case 'D':
			dmn=1;
			break;
     
		case 'v':
			version=1;
			break;
	       
		case 'p':
			port=atoi(optarg);
			break;
	       
		case 'm':
			creamport=atoi(optarg);
			break;
	       
		case 'b':
			binpath=strdup(optarg);
			break;
	       
		case 's':
			spooldir=strdup(optarg);
			break;
	       
		case 'l':
			debuglogname=strdup(optarg);
			break;
    	       
		case 'd':
			debug=atoi(optarg);
			break;
    	       
		case '?':
			break;
     
		default:
			abort ();
		}
	}
	
	if(help){
		usage();
	}
	 
	if(short_help){
		short_usage();
	}
	
	if(version) {
		printf("%s Version: %s\n",progname,VERSION);
		exit(EXIT_SUCCESS);
	}
	
	if(debug <=0){
		debug=0;
	}
	
	if(debug){
		if((debuglogfile = fopen(debuglogname, "a+"))==0){
			debuglogfile =  fopen("/dev/null", "a+");
		}
	}
    
	if(port) {
		if ( port < 1 || port > 65535) {
			sysfatal("Invalid port supplied: %r");
		}
	}else{
		port=DEFAULT_PORT;
    	}	

	if(creamport){
		usecream=1;
	}
 
	/* Get log dir name */

	ldir=make_message("%s/server_logs",spooldir);

	if (opendir(ldir)==NULL){
		do_log(debuglogfile, debug, 1, "dir %s does not exist or is not readable trying to get spool dir from pbs commands",ldir);
		tspooldir=GetPBSSpoolPath(binpath);
		free(ldir);
		ldir=make_message("%s/server_logs",tspooldir);
		free(spooldir);
		spooldir=strdup(tspooldir);
		if (opendir(ldir)==NULL){
			do_log(debuglogfile, debug, 1, "dir %s does not exist or is not readable (using pbs commands)",ldir);
			sysfatal("dir %s does not exist or is not readable (using pbs commands): %r",ldir);
		}
	}

	now=time(NULL);
	tptr=localtime(&now);
	strftime(cnow,sizeof(cnow),"%Y%m%d",tptr);

	eventsfile=make_message("%s/%s",ldir,cnow);
    
	/* Set to zero all the cache */
    
	for(j=0;j<RDXHASHSIZE;j++){
		rptr[j]=0;
	}
	for(j=0;j<CRMHASHSIZE;j++){
		nti[j]=0;
	}
    
	/*  Create the listening socket  */

	ai_req.ai_flags = AI_PASSIVE;
	ai_req.ai_family = PF_UNSPEC;
	ai_req.ai_socktype = SOCK_STREAM;
	ai_req.ai_protocol = 0; /* Any stream protocol is OK */

	sprintf(ainfo_port_string,"%5d",port);

	if (getaddrinfo(NULL, ainfo_port_string, &ai_req, &ai_ans) != 0) {
		sysfatal("Error getting address of passive SOCK_STREAM socket: %r");
	}

	address_found = 0;
	for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next) {

		if ((list_s = socket(cur_ans->ai_family,
					cur_ans->ai_socktype,
					cur_ans->ai_protocol)) == -1)
		{
			continue;
		}

		if(setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
			close(list_s);
			syserror("setsockopt() failed: %r");
		}

		if (bind(list_s,cur_ans->ai_addr, cur_ans->ai_addrlen) == 0) 
		{
			address_found = 1;
			break;
		}
		close(list_s);
	}
	freeaddrinfo(ai_ans);

	/*  Create the listening socket  */

	if ( address_found == 0 ) {
		sysfatal("Error creating and binding socket: %r");
	}
	if ( listen(list_s, LISTENQ) < 0 ) {
		sysfatal("Error calling listen() in main: %r");
	}
    
	/* create listening socket for Cream */
    
	if(usecream>0){
      
		if ( !creamport ) {
			sysfatal("Invalid port supplied for Cream: %r");
		}

		ai_req.ai_flags = AI_PASSIVE;
		ai_req.ai_family = PF_UNSPEC;
		ai_req.ai_socktype = SOCK_STREAM;
		ai_req.ai_protocol = 0; /* Any stream protocol is OK */

		sprintf(ainfo_port_string,"%5d",creamport);

		if (getaddrinfo(NULL, ainfo_port_string, &ai_req, &ai_ans) != 0) {
			sysfatal("Error getting address of passive SOCK_STREAM socket: %r");
		}

		address_found = 0;
		for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next) {

			if ((list_c = socket(cur_ans->ai_family,
					     cur_ans->ai_socktype,
					     cur_ans->ai_protocol)) == -1)
			{
				continue;
			}

			if(setsockopt(list_c, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
				close(list_c);
				syserror("setsockopt() failed: %r");
			}

			if (bind(list_c,cur_ans->ai_addr, cur_ans->ai_addrlen) == 0) 
			{
				address_found = 1;
				break;
			}
			close(list_c);
		}
		freeaddrinfo(ai_ans);

		/*  Create the listening socket  */

		if ( address_found == 0 ) {
			sysfatal("Error creating and binding CREAM socket: %r");
		}
		if ( listen(list_c, LISTENQ) < 0 ) {
			sysfatal("Error calling listen() in main: %r");
		}
	}

	if(dmn){    
		daemonize();
	}
    
	for(i=0;i<NUMTHRDS;i++){
		pthread_create(&ReadThd[i], NULL, (void *(*)(void *))LookupAndSend, (void *)list_s);
	}

	if(usecream>0){
		pthread_create(&CreamThd, NULL, (void *(*)(void *))CreamConnection, (void *)list_c);
	}
    
	pthread_create(&UpdateThd, NULL, mytail, (void *)eventsfile);
	pthread_join(UpdateThd, (void **)&status);
    
	pthread_exit(NULL);
 
}

/*---Functions---*/

void *
mytail (void *infile)
{    
        
	char *linebuffer;
    
	if((linebuffer=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc linebuffer: %r");
	}
    
	follow((char *)infile, linebuffer);
   
	return 0;
}

void
follow(char *infile, char *line)
{
	FILE *fp;
	long off = 0;
	long real_off = 0;

	char   *tdir;
	time_t lnow;
	struct tm *timeptr;
	char   *tnow;
	char   *evfile;
	char   *actualfile;
   
	if((tdir=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc tdir: %r");
	}
    
	if((evfile=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc evfile: %r");
	}

	if((tnow=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc tnow: %r");
	}
    
	strcat(tdir,spooldir);
	strcat(tdir,"/server_logs");

	actualfile=strdup(infile);
	
	for(;;){

/* In each cycle a new date file is costructed and is tested with the existing one
   when the date changes the new log file can be created later so we test if it is there
*/
     
		lnow=time(NULL);
		timeptr=localtime(&lnow);
		strftime(tnow,NUM_CHARS,"%Y%m%d",timeptr);
	
		evfile[0]='\0';
		strcat(evfile,tdir);
		strcat(evfile,"/");
		strcat(evfile,tnow);
		
		if(evfile && strcmp(evfile,actualfile) != 0){

			off = 0;
			actualfile=strdup(evfile);

			while(1){
				if((fp=fopen((char *)evfile, "r")) != 0){
					break;
				}
				sleep (1);
			}

		}else{

			if((fp=fopen((char *)actualfile, "r")) == 0){
				syserror("error opening %s: %r", actualfile);
				sleep(1);
				continue;
			}

		}

		if(fseek(fp, 0L, SEEK_END) < 0){
			sysfatal("couldn't seek in follow: %r");
		}
		real_off=ftell(fp);
	
		if(real_off < off){
			off=real_off;
		}
   
		if(fseek(fp, off, SEEK_SET) < 0){
			sysfatal("couldn't seek in follow: %r");
		}
        
		off = tail(fp, line, off);
		fclose(fp);
	
		sleep(1);
	}        
}

long
tail(FILE *fp, char *line, long old_off)
{
	long act_off=old_off;

	while(fgets(line, STR_CHARS, fp)){
		if (strrchr (line, '\n') == NULL){
			return act_off;
		}
		if(line && ((strstr(line,rex_queued)!=NULL) || (strstr(line,rex_running)!=NULL) || (strstr(line,rex_deleted)!=NULL) || (strstr(line,rex_finished)!=NULL) || (strstr(line,rex_unable)!=NULL) || (strstr(line,rex_hold)!=NULL) || (strstr(line,rex_dequeue)!=NULL && strstr(line,rex_staterun)!=NULL))){
			do_log(debuglogfile, debug, 2, "Tail line:%s",line);
			AddToStruct(line,1);
		}
		if((act_off=ftell(fp)) < 0){
			sysfatal("couldn't ftell in tail: %r");
		}
	}

	return act_off;
}

int
InfoAdd(int id, char *value, const char * flag)
{

	if(!value || (strlen(value)==0) || (strcmp(value,"\n")==0)){
		return -1;
	}
  
	do_log(debuglogfile, debug, 1, "Adding: ID:%d Type:%s Value:%s\n",rptr[id],flag,value);
	
	/* set write lock */
	pthread_mutex_lock( &write_mutex );
  
	if((strcmp(flag,"JOBID")==0) && j2js[id] == NULL){

		j2js[id] = strdup("1");
		j2bl[id] = strdup("\0");
		j2ec[id] = strdup("\0");
		j2st[id] = strdup("\0");
		j2rt[id] = strdup("\0");
		j2ct[id] = strdup("\0");
		
		reccnt[id] = recycled;
  
	} else if((strcmp(flag,"JOBID")==0) && recycled>0){
 
		free(j2js[id]);
		free(j2bl[id]);
		free(j2ec[id]);
		free(j2st[id]);
		free(j2rt[id]);
		free(j2ct[id]);
  
		j2js[id] = strdup("1");  
		j2bl[id] = strdup("\0");
		j2ec[id] = strdup("\0");
		j2st[id] = strdup("\0");
		j2rt[id] = strdup("\0");
		j2ct[id] = strdup("\0");
		
		reccnt[id] = recycled;
    
	} else if(strcmp(flag,"BLAHPNAME")==0){
 
		free(j2bl[id]);
		j2bl[id] = strdup(value);
  
	} else if(strcmp(flag,"JOBSTATUS")==0){
 
		free(j2js[id]);
		j2js[id] = strdup(value);
 
	} else if(strcmp(flag,"EXITCODE")==0){

		free(j2ec[id]);
		j2ec[id] = strdup(value);
 
	} else if(strcmp(flag,"STARTTIME")==0){

		free(j2st[id]);
		j2st[id] = strdup(value);
 
	} else if(strcmp(flag,"RUNNINGTIME")==0){
		free(j2rt[id]);
		j2rt[id] = strdup(value);
 
	} else if(strcmp(flag,"COMPLTIME")==0){

		free(j2ct[id]);
		j2ct[id] = strdup(value);
 
	} else {
 
		/* release write lock */
		pthread_mutex_unlock( &write_mutex );
    
		return -1;
 
	}
	/* release write lock */
	pthread_mutex_unlock( &write_mutex );
    
	return 0;
}

int
AddToStruct(char *line, int flag)
{

	int has_blah=0;
 
	char *trex;
	char *rex;

	int  maxtok; 
	char **tbuf;
 
	int id=-1;
	int is_queued=0;
	int is_finished=0;
	int belongs_to_current_cycle;

	char *tjobid=NULL;
	char *jobid=NULL;

	char *tj_time=NULL;
	char *j_time=NULL;

	char *tex_status=NULL;
	char *ex_status=NULL;

	char *tb_job=NULL;
	char *tj_blahjob=NULL;
	char *j_blahjob=NULL;

	if(line && ((strstr(line,blahjob_string)!=NULL) || (strstr(line,bl_string)!=NULL) || (strstr(line,cream_string)!=NULL))){
		has_blah=1;
	}
 
	maxtok=strtoken(line,';',&tbuf);
  
	if(maxtok>0){
		tj_time=strdup(tbuf[0]);
		j_time=convdate(tj_time);
	}
	if(maxtok>4){
		tjobid=strdup(tbuf[4]);
	}
	if(maxtok>5){
		rex=strdup(tbuf[5]);
		trex=strdup(rex);
	}

	freetoken(&tbuf,maxtok);

	/* get jobid */ 

	if(tjobid){

		maxtok=strtoken(tjobid,'.',&tbuf);
		if(maxtok>0){
			jobid=strdup(tbuf[0]);
		}
		freetoken(&tbuf,maxtok);
	
	} /* close tjobid if */

  
	/* get j_blahjob */

	if(rex && (strstr(rex,rex_queued)!=NULL)){
 		 is_queued=1; 

		maxtok=strtoken(trex,',',&tbuf);
		if(maxtok>2){
			tb_job=strdup(tbuf[2]);
		}
		freetoken(&tbuf,maxtok);

		maxtok=strtoken(tb_job,'=',&tbuf);
		if(maxtok>1){
			tj_blahjob=strdup(tbuf[1]);
		}
		freetoken(&tbuf,maxtok);

		maxtok=strtoken(tj_blahjob,' ',&tbuf);
		if(maxtok>0){
			j_blahjob=strdup(tbuf[0]);
		}else{
			j_blahjob=strdup(tj_blahjob);
		}
		freetoken(&tbuf,maxtok);
   
	} /* close rex_queued if */

	/* get ex_status */
	if(rex && (strstr(rex,rex_finished)!=NULL)){
		is_finished=1;
  
		maxtok=strtoken(trex,' ',&tbuf);
		if(maxtok>0){
			tex_status=strdup(tbuf[0]);
		}
		freetoken(&tbuf,maxtok);
  
		maxtok=strtoken(tex_status,'=',&tbuf);
		if(maxtok>1){
			ex_status=strdup(tbuf[1]);
		}
		freetoken(&tbuf,maxtok);
  
	} /* close rex_finished if */
	
	if(jobid){ 
		id=UpdatePtr(atoi(jobid),tjobid,is_queued,has_blah);
	}
	belongs_to_current_cycle = 0;
	if((id >= 0) && ((reccnt[id]==recycled) ||
	   ((id >= ptrcnt) && (reccnt[id]==(recycled-1)))))
		belongs_to_current_cycle = 1;

	if((id >= 0) && (is_queued==1) && (has_blah)){

		InfoAdd(id,jobid,"JOBID");
		InfoAdd(id,j_time,"STARTTIME");
		InfoAdd(id,j_blahjob,"BLAHPNAME");

		if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
			NotifyCream(id, "1", j2bl[id], "NA", "NA", j2st[id], flag);
		}  

	} else if((id >= 0) && (belongs_to_current_cycle) && (j2bl[id]) && ((strstr(j2bl[id],blahjob_string)!=NULL)  || (strstr(j2bl[id],bl_string)!=NULL) || (strstr(j2bl[id],cream_string)!=NULL))){ 
 
		if(rex && strstr(rex,rex_running)!=NULL){

			InfoAdd(id,"2","JOBSTATUS");
			InfoAdd(id,j_time,"RUNNINGTIME");
   
			if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
				NotifyCream(id, "2", j2bl[id], "NA", "NA", j2rt[id], flag);
			}
   
 		 } else if(rex && strstr(rex,rex_unable)!=NULL){
  
   			InfoAdd(id,"1","JOBSTATUS");

			if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
				NotifyCream(id, "1", j2bl[id], "NA", "NA", j_time, flag);
			}
    
 		 } else if(rex && (strstr(rex,rex_deleted)!=NULL || (strstr(line,rex_dequeue)!=NULL && strstr(line,rex_staterun)!=NULL))){
  
   			InfoAdd(id,"3","JOBSTATUS");

			if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
				NotifyCream(id, "3", j2bl[id], "NA", "NA", j_time, flag);
			}

		} else if(is_finished==1){
  
			InfoAdd(id,"4","JOBSTATUS");
			InfoAdd(id,ex_status,"EXITCODE");
			InfoAdd(id,j_time,"COMPLTIME");

			if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
				NotifyCream(id, "4", j2bl[id], "NA", ex_status, j2ct[id], flag);
			}

		} else if(rex && ((strstr(rex,rex_uhold)!=NULL) || (strstr(rex,rex_ohold)!=NULL) || (strstr(rex,rex_ohold)!=NULL))){
   
			if(j2js[id] && strcmp(j2js[id],"1")==0){
				InfoAdd(id,"5","JOBSTATUS");
	                        if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					NotifyCream(id, "5", j2bl[id], "NA", "NA", j_time, flag);
				}
			}
   
		} else if(rex && ((strstr(rex,rex_uresume)!=NULL) || (strstr(rex,rex_oresume)!=NULL) || (strstr(rex,rex_oresume)!=NULL))){
   
			if(j2js[id] && strcmp(j2js[id],"5")==0){
				InfoAdd(id,"1","JOBSTATUS");
				if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					NotifyCream(id, "1", j2bl[id], "NA", "NA", j_time, flag);
				}
			}
   
		} /* closes if-else if on rex_ */
	} /* closes if-else if on jobid lookup */
 
	free(rex);
	free(trex);
	free(tj_time);
	free(j_time);
	free(tjobid);
	free(jobid);
	free(tex_status);
	free(ex_status);
	free(tb_job);
	free(tj_blahjob);
	free(j_blahjob);


	return 0;
}

char *
GetAllEvents(char *file)
{
 
	FILE *fp;
	char *line;
	char **opfile=NULL;
	int i=0;
	int maxtok;
 
	maxtok = strtoken(file, ' ', &opfile);

	if((line=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc line: %r");
	}
  
	for(i=0; i<maxtok; i++){ 
 
		if((fp=fopen(opfile[i], "r")) != 0){
			while(fgets(line, STR_CHARS, fp)){
				if(line && ((strstr(line,rex_queued)!=NULL) || (strstr(line,rex_running)!=NULL) || (strstr(line,rex_deleted)!=NULL) || (strstr(line,rex_finished)!=NULL) || (strstr(line,rex_unable)!=NULL) || (strstr(line,rex_hold)!=NULL) || (strstr(line,rex_dequeue)!=NULL && strstr(line,rex_staterun)!=NULL))){
					AddToStruct(line,0);
				}
			}
			fclose(fp);
		} else {
			syserror("Cannot open %s file: %r",opfile[i]);
		}

	} /* close for*/
	
	freetoken(&opfile,maxtok);
	   
	free(file);
	free(line);

	return NULL;

}

void *
LookupAndSend(int m_sock)
{ 
    
	char      *buffer=NULL;
	char      *out_buf=NULL;
	char      *logdate=NULL;
	char      *jobid=NULL;
	char      *jstat=NULL;
	char      *pr_removal="Not";
	int       i,maxtok;
	int       id;
	int       bid;
	int       conn_s;
	char      **tbuf;
	char      *cp;
	char      *irptr;
	int       listcnt=0;
	int       listbeg=0;
	char      *buftmp=NULL;
    
	while ( 1 ) {


		/*  Wait for a connection, then accept() it  */
	
		if ( (conn_s = accept(m_sock, NULL, NULL) ) < 0 ) {
			sysfatal("Error calling accept(): %r");
		}

		if((buffer=calloc(STR_CHARS,1)) == 0){
			sysfatal("can't malloc buffer in LookupAndSend: %r");
		}
	
		Readline(conn_s, buffer, STR_CHARS-1);
		buftmp=strdel(buffer,"\n");
		do_log(debuglogfile, debug, 3, "Received:%s\n",buftmp);
		free(buftmp);
	
		/* printf("thread/0x%08lx\n",pthread_self()); */
	
		if((strlen(buffer)==0) || (strcmp(buffer,"\n")==0) || (strstr(buffer,"/")==0) || (strcmp(buffer,"/")==0)){
			out_buf=make_message("\n");
			goto close;
		}
    
		if ((cp = strrchr (buffer, '\n')) != NULL){
			*cp = '\0';
		}

		maxtok=strtoken(buffer,'/',&tbuf);
		if(tbuf[0]){
			logdate=strdup(tbuf[0]);
		}
		if(tbuf[1]){
			jobid=strdup(tbuf[1]);
		}
		freetoken(&tbuf,maxtok);

		
/* HELP reply */
       
		if(logdate && strcmp(logdate,"HELP")==0){
			out_buf=make_message("Commands: BLAHJOB/<blahjob-id> <date-YYYYmmdd>/<jobid> HELP TEST VERSION CREAMPORT TOTAL LISTALL LISTF[/<first-n-jobid>] LISTL[/<last-n-jobid>]\n");
			goto close;
		}

/* TEST reply */

		if(logdate && strcmp(logdate,"TEST")==0){
			out_buf=make_message("YPBS\n");
			goto close;
		}

/* VERSION reply */
       
		if(logdate && strcmp(logdate,"VERSION")==0){
			out_buf=make_message("%s\n",VERSION);
			goto close;
		}
	
/* TOTAL reply */
       
		if(logdate && strcmp(logdate,"TOTAL")==0){
			if(recycled){
				out_buf=make_message("Total number of jobs:%d\n",RDXHASHSIZE);
			}else{
				out_buf=make_message("Total number of jobs:%d\n",ptrcnt-1);
			}
			goto close;
		}
	
/* LISTALL reply */
       
		if(logdate && strcmp(logdate,"LISTALL")==0){
			if((out_buf=calloc(MAX_CHARS*3,1)) == 0){
				sysfatal("can't malloc out_buf in LookupAndSend: %r");
			}
			if((irptr=calloc(STR_CHARS,1)) == 0){
				sysfatal("can't malloc irptr in LookupAndSend: %r");
			}
			if(recycled){
				for(i=ptrcnt;i<RDXHASHSIZE;i++){
					sprintf(irptr,"%d",rptr[i]);
					strcat(out_buf,irptr);
					strcat(out_buf," ");
				}
			}
			for(i=1;i<ptrcnt;i++){
				sprintf(irptr,"%d",rptr[i]);
				strcat(out_buf,irptr);
				strcat(out_buf," ");
			}
			free(irptr);
			strcat(out_buf,"\n");
			goto close;
		}

/* LISTF reply */
       
		if(logdate && strcmp(logdate,"LISTF")==0){
			if((out_buf=calloc(MAX_CHARS*3,1)) == 0){
				sysfatal("can't malloc out_buf in LookupAndSend: %r");
			}
	 
			if((listcnt=atoi(jobid))<=0){
				listcnt=10;
			}
			if(listcnt>ptrcnt-1){
				listcnt=ptrcnt-1;
			}
			if((irptr=calloc(STR_CHARS,1)) == 0){
				sysfatal("can't malloc irptr in LookupAndSend: %r");
			}
			sprintf(out_buf,"List of first %d jobid:",listcnt);
			if(recycled){
				for(i=ptrcnt;i<ptrcnt+listcnt;i++){
					sprintf(irptr,"%d",rptr[i]);
					strcat(out_buf,irptr);
					strcat(out_buf," ");
				}
			}else{
				for(i=1;i<=listcnt;i++){
					sprintf(irptr,"%d",rptr[i]);
					strcat(out_buf,irptr);
					strcat(out_buf," ");
				}
			}
			free(irptr);
			strcat(out_buf,"\n");
			goto close;
		}

/* LISTL reply */
       
		if(logdate && strcmp(logdate,"LISTL")==0){
			if((out_buf=calloc(MAX_CHARS*3,1)) == 0){
				sysfatal("can't malloc out_buf in LookupAndSend: %r");
			}
	 
			if((listcnt=atoi(jobid))<=0){
				listcnt=10;
			}
			if(ptrcnt-listcnt>0){
				listbeg=ptrcnt-listcnt;
			}else{
				listbeg=1;
				listcnt=ptrcnt-1;
			}
	 
			if((irptr=calloc(STR_CHARS,1)) == 0){
				sysfatal("can't malloc irptr in LookupAndSend: %r");
			}
			sprintf(out_buf,"List of last %d jobid:",listcnt);
			if(recycled){
				for(i=RDXHASHSIZE+(ptrcnt-listcnt);i<RDXHASHSIZE;i++){
					sprintf(irptr,"%d",rptr[i]);
					strcat(out_buf,irptr);
					strcat(out_buf," ");
				}
			}
			for(i=listbeg;i<ptrcnt;i++){
				sprintf(irptr,"%d",rptr[i]);
				strcat(out_buf,irptr);
				strcat(out_buf," ");
			}
			free(irptr);
			strcat(out_buf,"\n");
			goto close;
		}

/* get port where the parser is waiting for a connection from cream and send it to cream */
       
		if(logdate && strcmp(logdate,"CREAMPORT")==0){
			out_buf=make_message("%d\n",creamport);
			goto close;
		}

/* get jobid from blahjob id (needed by *_submit.sh) */
       
		if(logdate && strcmp(logdate,"BLAHJOB")==0){
			for(i=0;i<WRETRIES;i++){
				bid=GetBlahNameId(jobid);
				pthread_mutex_lock(&write_mutex);
				if(bid==-1){
					pthread_mutex_unlock(&write_mutex);
					sleep(1);
					continue;
				}
				out_buf=make_message("%s\n",rfullptr[bid]);
				pthread_mutex_unlock(&write_mutex);
				goto close;
			}
			if(i==WRETRIES){
				out_buf=make_message("\n");
				goto close;
			}
		}
	
		if((strlen(logdate)==0) || (strcmp(logdate,"\n")==0)){
			out_buf=make_message("\n");
			goto close;
		}
	
/* get all info from jobid */

		id=GetRdxId(atoi(jobid));
	
		pthread_mutex_lock(&write_mutex);
		if(id>0 && j2js[id]!=NULL){
 		 
			/* This proxy removal message is now ignored by the
			 * lsf_status.sh script, since we no longer make symlinks in
			 * ~/.blah_jobproxy_dir.
			 */
			if(j2js[id] && ((strcmp(j2js[id],"3")==0) || (strcmp(j2js[id],"4")==0))){
				pr_removal="Yes";
			} else {
				pr_removal="Not";
			}
			jstat=make_message(" JobStatus=%s;",j2js[id]);
	 
			if(j2js[id] && strcmp(j2js[id],"4")==0){
				out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\"; LRMSCompletedTime=\"%s\"; JwExitCode=%s;/%s\n",jobid, jstat, j2st[id], j2rt[id], j2ct[id], j2ec[id], pr_removal);
			}else if(j2rt[id] && strcmp(j2rt[id],"\0")!=0){
				out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\";/%s\n",jobid, jstat, j2st[id], j2rt[id], pr_removal);
			}else{
				out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\";/%s\n",jobid, jstat, j2st[id], pr_removal);
			}
			pthread_mutex_unlock(&write_mutex);
	 
		} else {
	
			pthread_mutex_unlock(&write_mutex);
			GetEventsInOldLogs(logdate);
	 
			id=GetRdxId(atoi(jobid));
	 
			pthread_mutex_lock(&write_mutex);
			if(id>0 && j2js[id]!=NULL){

				if((out_buf=calloc(STR_CHARS,1)) == 0){
					sysfatal("can't malloc out_buf in LookupAndSend: %r");
				}
	  
				/* This proxy removal message is now ignored by the
				 * lsf_status.sh script, since we no longer make symlinks in
				 * ~/.blah_jobproxy_dir.
				 */
				if(j2js[id] && ((strcmp(j2js[id],"3")==0) || (strcmp(j2js[id],"4")==0))){
					pr_removal="Yes";
				} else {
					pr_removal="Not";
				}
				
				jstat=make_message(" JobStatus=%s;",j2js[id]);
	  
				if(j2js[id] && strcmp(j2js[id],"4")==0){
					out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\"; LRMSCompletedTime=\"%s\"; JwExitCode=%s;/%s\n",jobid, jstat, j2st[id], j2rt[id], j2ct[id], j2ec[id], pr_removal);
				}else if(j2rt[id] && strcmp(j2rt[id],"\0")!=0){
					out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\";/%s\n",jobid, jstat, j2st[id], j2rt[id], pr_removal);
				}else{
					out_buf=make_message("[BatchJobId=\"%s\";%s LRMSSubmissionTime=\"%s\";/%s\n",jobid, jstat, j2st[id], pr_removal);
				}
				pthread_mutex_unlock(&write_mutex);
	  
			} else {
				pthread_mutex_unlock(&write_mutex);
				out_buf=make_message("JobId %s not found/Not\n",jobid);
			}
	 
		}
	
close:	
		Writeline(conn_s, out_buf, strlen(out_buf));
		do_log(debuglogfile, debug, 3, "Sent:%s",out_buf);

		free(out_buf);
		free(buffer);
		free(logdate);
		free(jobid);
		free(jstat);
	
		/*  Close the connected socket  */

		if ( close(conn_s) < 0 ) {
			sysfatal("Error calling close(): %r");
		}
	
	} /* closes while */
	
	return 0; 
}

int
GetEventsInOldLogs(char *logdate)
{

	char *loglist=NULL;
 
	loglist=GetLogList(logdate);
 
	if(loglist!=NULL){
		GetAllEvents(loglist);
	}
 
	return 0;
 
}

char *
GetLogList(char *logdate)
{
	struct dirent   **direntry;
	int             rc;
	struct stat     sbuf;
	time_t          tage;
	char            *s,*p;
	struct tm       tmthr;
	char            *slogs;
	int 		i,n;

	if((slogs=calloc(MAX_CHARS*3,1)) == 0){
		sysfatal("can't malloc slogs: %r");
	}
	 
	tmthr.tm_sec=tmthr.tm_min=tmthr.tm_hour=tmthr.tm_isdst=0;
	p=strptime(logdate,"%Y%m%d%H%M.%S",&tmthr);
	if( (p-logdate) != 15) {
		do_log(debuglogfile, debug, 1, "Timestring \"%s\" is invalid (YYYYmmddhhmm.ss)\n",logdate);
		syserror("Timestring \"%s\" is invalid (YYYYmmddhhmm.ss): %r", logdate);
		return NULL;
	}
	tage=mktime(&tmthr);
	
	n = scandir(ldir, &direntry, 0, alphasort);
	if (n < 0){
		syserror("scandir error: %r");
		return NULL;
	} else {
		for (i = 0; i < n; i++) {
			if( *(direntry[i]->d_name) == '.' ) continue;
			s=make_message("%s/%s",ldir,direntry[i]->d_name);
			rc=stat(s,&sbuf);
			if(rc) {
				syserror("Cannot stat file %s: %r", s);
				return NULL;
			}
			if ( sbuf.st_mtime > tage ) {
				strcat(slogs,s);
				strcat(slogs," ");
			}  
			free(s);
			free(direntry[i]);
		}
		free(direntry);
	}
	 
	do_log(debuglogfile, debug, 1, "Log list:%s\n",slogs);
	
	return(slogs);

}

void
CreamConnection(int c_sock)
{ 

	char      *buffer;
	char     *buftmp;

	if((buffer=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc buffer in CreamConnection: %r");
	}

	while ( 1 ) {
	  
		do_log(debuglogfile, debug, 1, "Listening for new connection in CreamConnection\n");
		if ( (conn_c = accept(c_sock, NULL, NULL) ) < 0 ) {
			do_log(debuglogfile, debug, 1, "Fatal Error:Error calling accept() on c_sock in CreamConnection\n");
			sysfatal("Error calling accept() in CreamConnection: %r");
		}
		while ( 1 ) {
		
			*buffer = 0;
			if(Readline(conn_c, buffer, STR_CHARS-1)<=0){
				close(conn_c);
				break;
			}
			if(strlen(buffer)>0){
				buftmp=strdel(buffer,"\n");
				do_log(debuglogfile, debug, 1, "Received for Cream:%s\n",buftmp);
				free(buftmp);
				if(buffer && ((strstr(buffer,"STARTNOTIFY/")!=NULL) || (strstr(buffer,"STARTNOTIFYJOBLIST/")!=NULL) || (strstr(buffer,"STARTNOTIFYJOBEND/")!=NULL) || (strstr(buffer,"CREAMFILTER/")!=NULL))){
					NotifyFromDate(buffer);
				}else if(buffer && (strstr(buffer,"PARSERVERSION/")!=NULL)){
					GetVersion();
				}
			}
		}
	}
}

int 
GetVersion()
{

	char *out_buf;
	
	out_buf=make_message("%s__0\n",VERSION);
	Writeline(conn_c, out_buf, strlen(out_buf));
	do_log(debuglogfile, debug, 1, "Sent Reply for PARSERVERSION command:%s",out_buf);
	free(out_buf);
	
	return 0;
	
}

int
NotifyFromDate(char *in_buf)
{

	char *out_buf=NULL;
	int    ii;
	char *notstr=NULL;
	char *notstrshort=NULL;
	char *notdate=NULL;
	int   notepoch;
	int   logepoch;

	int  maxtok,maxtok_s,maxtok_l,maxtok_b; 
	char **tbuf;
	char **sbuf;
	char **lbuf;
	char **bbuf;
	char *fullblahstring=NULL;
	char *joblist_string=NULL;
	char *tjoblist_string=NULL;
	char *fullbljobid=NULL;
	char *tbljobid=NULL;
	char *cp;

	maxtok=strtoken(in_buf,'/',&tbuf);
    
	if(tbuf[0]){
		notstr=strdup(tbuf[0]);
	}
	if(tbuf[1]){
		notdate=strdup(tbuf[1]);
		if ((cp = strrchr (notdate, '\n')) != NULL){
			*cp = '\0';
		}
	}
    
	freetoken(&tbuf,maxtok);
	
/*if CREAMFILTER is sent this string is used instead of default cream_string */

	if(notstr && strcmp(notstr,"CREAMFILTER")==0){
		cream_string=strdup(notdate);
                if ((cp = strrchr (cream_string, '\n')) != NULL){
                        *cp = '\0';
                }
                if ((cp = strrchr (cream_string, '\r')) != NULL){
                        *cp = '\0';
                }
		if(cream_string!=NULL){
			out_buf=make_message("CREAMFILTER set to %s\n",cream_string);
		}else{
			out_buf=make_message("CREAMFILTER ERROR\n");
		}
		
		Writeline(conn_c, out_buf, strlen(out_buf));
		do_log(debuglogfile, debug, 1, "Sent Reply for CREAMFILTER command:%s",out_buf);
		
	}else if(notstr && strcmp(notstr,"STARTNOTIFY")==0){
    
		creamisconn=1;
      
		notepoch=str2epoch(notdate,"S");
		notstrshort=iepoch2str(notepoch,"L");      
      
		if(cream_recycled){
			logepoch=nti[jcount];
		}else{
			if(nti[0]==0){
				logepoch=time(NULL);
			}else{
				logepoch=nti[0];
			}
		}
      
		if(notepoch<=logepoch){
			GetEventsInOldLogs(notstrshort);
		}
      
		if(cream_recycled){

			for(ii=jcount;ii<CRMHASHSIZE;ii++){
				if(notepoch<=nti[ii]){
					fullblahstring=make_message("BlahJobName=\"%s",cream_string);
					if(ntf[ii] && strstr(ntf[ii],fullblahstring)!=NULL){
						out_buf=make_message("NTFDATE/%s",ntf[ii]);
						Writeline(conn_c, out_buf, strlen(out_buf));
						do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:%s",out_buf);
					}
				}
			}

		}
            
		for(ii=0;ii<jcount;ii++){
			if(notepoch<=nti[ii]){
				fullblahstring=make_message("BlahJobName=\"%s",cream_string);
				if(ntf[ii] && strstr(ntf[ii],fullblahstring)!=NULL){
					out_buf=make_message("NTFDATE/%s",ntf[ii]); 
					Writeline(conn_c, out_buf, strlen(out_buf));
					do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:%s",out_buf);
				}
			}
		}
		Writeline(conn_c, "NTFDATE/END\n", strlen("NTFDATE/END\n"));
		do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:NTFDATE/END\n");
      
		free(out_buf);
		free(notstr);
		free(notstrshort);
		free(notdate);
		free(fullblahstring);
      
		return 0;
	
	}else if(notstr && strcmp(notstr,"STARTNOTIFYJOBEND")==0){

		creamisconn=1;
		
		Writeline(conn_c, "NTFDATE/END\n", strlen("NTFDATE/END\n"));
		do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:NTFDATE/END\n");
		
	}else if(notstr && strcmp(notstr,"STARTNOTIFYJOBLIST")==0){
    
		creamisconn=1;
		
		maxtok_s=strtoken(notdate,';',&sbuf);
		
		notepoch=str2epoch(sbuf[0],"S");
		notstrshort=iepoch2str(notepoch,"L");
		if(sbuf[1]){     
			tjoblist_string=strdup(sbuf[1]);
		}
		
		joblist_string=make_message(",%s,",tjoblist_string);
		
		freetoken(&sbuf,maxtok_s);
		
		if(cream_recycled){
			logepoch=nti[jcount];
		}else{
			if(nti[0]==0){
				logepoch=time(NULL);
			}else{
				logepoch=nti[0];
			}
		} 
		if(logepoch<=0){
			logepoch=time(NULL);
		}     
		if(notepoch<=logepoch){
			GetEventsInOldLogs(notstrshort);
		}
      
		if(cream_recycled){

			for(ii=jcount;ii<CRMHASHSIZE;ii++){
				if(notepoch<=nti[ii]){
					
					maxtok_l=strtoken(ntf[ii],';',&lbuf);
					maxtok_b=strtoken(lbuf[2],'=',&bbuf);
					tbljobid=strdel(bbuf[1],"\"");
					fullbljobid=make_message(",%s,",tbljobid);
					free(tbljobid);
					
					freetoken(&lbuf,maxtok_l);
					freetoken(&bbuf,maxtok_b);
					
					if(ntf[ii] && strstr(joblist_string,fullbljobid)!=NULL){
						out_buf=make_message("NTFDATE/%s",ntf[ii]);
						Writeline(conn_c, out_buf, strlen(out_buf));
						do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:%s",out_buf);
					}
					free(fullbljobid);
				}
			}

		}
            
		for(ii=0;ii<=jcount;ii++){
			if(notepoch<=nti[ii]){
				maxtok_l=strtoken(ntf[ii],';',&lbuf);
				maxtok_b=strtoken(lbuf[2],'=',&bbuf);
				tbljobid=strdel(bbuf[1],"\"");
				fullbljobid=make_message(",%s,",tbljobid);
				free(tbljobid);
				
				freetoken(&lbuf,maxtok_l);
				freetoken(&bbuf,maxtok_b);
				
				if(ntf[ii] && strstr(joblist_string,fullbljobid)!=NULL){
					out_buf=make_message("NTFDATE/%s",ntf[ii]);
					Writeline(conn_c, out_buf, strlen(out_buf));
					do_log(debuglogfile, debug, 1, "Sent for Cream_nftdate:%s",out_buf);
				}
				free(fullbljobid);
			}
		}
      
		free(out_buf);
		free(notstr);
		free(notstrshort);
		free(notdate);
		free(joblist_string);
		free(tjoblist_string);

		return 0;    
	}
    
	free(out_buf);
	free(notstr);
	free(notdate);
	free(fullblahstring);
    	    
	return -1;
}

int
NotifyCream(int jobid, char *newstatus, char *blahjobid, char *wn, char *reason, char *timestamp, int flag)
{

	/*if flag ==0 AddToStruct is called within GetOldLogs 
	  if flag ==1 AddToStruct is called elsewhere*/
   
	char     *buffer=NULL;
	char     *outreason=NULL;
	char     *exitreason=NULL;
	char     *sjobid=NULL;
  
	int      retcod;
        
	struct   pollfd fds[2];
	struct   pollfd *pfds;
	int      nfds = 1;
	int      timeout= 5000;
    
	char    **clientjobid;
	int      maxtok;
    
	fds[0].fd = conn_c;
	fds[0].events = 0;
	fds[0].events = ( POLLOUT | POLLPRI | POLLERR | POLLHUP | POLLNVAL ) ;
	pfds = fds;
    
	sjobid=make_message("%d",rptr[jobid]);
    
	if(reason && strcmp(reason,"NA")!=0){
		outreason=make_message(" Reason=\"pbs_reason=%s\";" ,reason);
		if(strcmp(reason,"271")==0){
			exitreason=make_message(" ExitReason=\"Killed by Resource Management System\";");
		}else{
			exitreason=make_message("");	
		}
	}else{
                outreason=make_message("");
                exitreason=make_message("");
        }
    
	maxtok = strtoken(blahjobid, '_', &clientjobid);    
    
	if(wn && strcmp(wn,"NA")!=0){
		buffer=make_message("[BatchJobId=\"%s\"; JobStatus=%s; BlahJobName=\"%s\"; ClientJobId=\"%s\"; WorkerNode=%s;%s%s ChangeTime=\"%s\";]\n",sjobid, newstatus, blahjobid, clientjobid[1], wn, outreason, exitreason, timestamp);
	}else{
		buffer=make_message("[BatchJobId=\"%s\"; JobStatus=%s; BlahJobName=\"%s\"; ClientJobId=\"%s\"; %s%s ChangeTime=\"%s\";]\n",sjobid, newstatus, blahjobid, clientjobid[1], outreason, exitreason, timestamp);
	}
    
	freetoken(&clientjobid,maxtok);

	free(sjobid);

	/* set lock for cream cache */
	pthread_mutex_lock( &cr_write_mutex );

	if(jcount>=CRMHASHSIZE){
		jcount=0;
		cream_recycled=1;
		do_log(debuglogfile, debug, 3, "Cream Counter Recycled\n");
	} 
    
	nti[jcount]=str2epoch(timestamp,"S");
	ntf[jcount++]=strdup(buffer);
    
	/* unset lock for cream cache */
	pthread_mutex_unlock( &cr_write_mutex );
    
	if((creamisconn==0) || (flag==0)){
		free(buffer);
		free(outreason);
		free(exitreason);
		return -1;
	}
        
	retcod = poll(pfds, nfds, timeout); 
        
	if(retcod <0){
		close(conn_c);
		do_log(debuglogfile, debug, 1, "Fatal Error:Poll error in NotifyCream\n");
		sysfatal("Poll error in NotifyCream: %r");
	}else if ( retcod == 0 ){
		do_log(debuglogfile, debug, 1, "Error:poll() timeout in NotifyCream\n");
		syserror("poll() timeout in NotifyCream: %r");
		return -1;
	}else if ( retcod > 0 ){
		if ( ( fds[0].revents & ( POLLERR | POLLNVAL | POLLHUP) )){
			switch (fds[0].revents){
			case POLLNVAL:
				do_log(debuglogfile, debug, 1, "Error:poll() file descriptor error in NotifyCream\n");
				syserror("poll() file descriptor error in NotifyCream: %r");
				break;
			case POLLHUP:
				do_log(debuglogfile, debug, 1, "Connection closed in NotifyCream\n");
				syserror("Connection closed in NotifyCream: %r");
				break;
			case POLLERR:
				do_log(debuglogfile, debug, 1, "Error:poll() POLLERR in NotifyCream\n");
				syserror("poll() POLLERR in NotifyCream: %r");
				break;
			}
		} else {
			Writeline(conn_c, buffer, strlen(buffer));
			do_log(debuglogfile, debug, 1, "Sent for Cream:%s",buffer);
		} 
	}       
			
	free(buffer);
	free(outreason);
	free(exitreason);
    
	return 0;
    
}

int
UpdatePtr(int jid,char *fulljobid,int is_que,int has_bl)
{

	int rid;

	if((jid < 0)){
		return -1;
	}

	/* if it is over RDXHASHSIZE the ptrcnt is recycled */
 
	if(ptrcnt>=RDXHASHSIZE){
		ptrcnt=1;
		recycled++;
		do_log(debuglogfile, debug, 3, "Counter Recycled\n");
	}

        if((rid=GetRdxId(jid))==-1){
	
		do_log(debuglogfile, debug, 3, "JobidNew Counter:%d jobid:%d\n",ptrcnt,jid);
		if((is_que) && (has_bl)){
			rptr[ptrcnt]=jid;
			if(recycled && rfullptr[ptrcnt]){

				free(rfullptr[ptrcnt]);
			}
			rfullptr[ptrcnt++]=strdup(fulljobid);
			return(ptrcnt-1);
		}else{
			return -1;		
		}
	}else{
		do_log(debuglogfile, debug, 3, "JobidOld Counter:%d jobid:%d\n",rid,jid);
		return rid;
	}

}

char *
convdate(char *date)
{
  
	char *dateout;

	struct tm tm;
	
	strptime(date,"%m/%d/%Y %H:%M:%S",&tm);
 
	if((dateout=calloc(NUM_CHARS,1)) == 0){
		sysfatal("can't malloc dateout in convdate: %r");
	}
 
	strftime(dateout,NUM_CHARS,"%Y-%m-%d %H:%M:%S",&tm);
 
	return dateout;
 
}

void
sighup()
{
	if(debug){
		fclose(debuglogfile);
		if((debuglogfile = fopen(debuglogname, "a+"))==0){
			debuglogfile =  fopen("/dev/null", "a+");
		}
	}	
}

int
usage()
{
	printf("Usage: BLParserPBS [OPTION...]\n");
	printf("  -p, --port=<port number>               port\n");
	printf("  -m, --creamport=<creamport number>     creamport\n");
	printf("  -b, --binpath=<PBSbinpath>             PBS binpath\n");
	printf("  -s, --spooldir=<PBSspooldir>           PBS spooldir\n");
	printf("  -l, --logfile =<DebugLogFile>          DebugLogFile\n");
	printf("  -d, --debug=INT                        enable debugging\n");
	printf("  -D, --daemon                           run as daemon\n");
	printf("  -v, --version                          print version and exit\n");
	printf("\n");
	printf("Help options:\n");
	printf("  -?, --help                             Show this help message\n");
	printf("  --usage                                Display brief usage message\n");
	exit(EXIT_SUCCESS);
}

int
short_usage()
{
	printf("Usage: BLParserPBS [-Dv?] [-p|--port <port number>]\n");
	printf("        [-m|--creamport <creamport number>] [-s|--spooldir <PBSspooldir>] [-b|--binpath <PBSbinpath>]\n");
	printf("        [-l|--logfile  <DebugLogFile>] [-d|--debug INT] [-D|--daemon]\n");
	printf("        [-v|--version] [-?|--help] [--usage]\n");
	exit(EXIT_SUCCESS);
}

