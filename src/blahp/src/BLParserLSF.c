/*
#  File:     BLParserLSF.c
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

#include "BLParserLSF.h"

int
main(int argc, char *argv[])
{

	int       set = 1;
	int       i,j;
	int       status;
	int       list_s;
	int       list_c;
	char ainfo_port_string[16];
	struct addrinfo ai_req, *ai_ans, *cur_ans;
	int address_found;


	pthread_t ReadThd[NUMTHRDS];
	pthread_t UpdateThd;
	pthread_t CreamThd;

	argv0 = argv[0];
    
	/*Ignore sigpipe*/
    
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP,sighup);

	/* Get log dir name and port from conf file*/

	ldir=GetLogDir(argc,argv);
        
	eventsfile=make_message("%s/%s",ldir,lsbevents);
    
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
    
	if((linebuffer=calloc(MAX_CHARS,1)) == 0){
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
	long tmp_off = 0;
	
	for(;;){
	
		if((fp=fopen((char *)infile, "r")) == 0){
			syserror("error opening %s: %r", infile);
			sleep(1);
			continue;
		}
		if(fseek(fp, 0L, SEEK_END) < 0){
			sysfatal("couldn't seek: %r");
		}
		real_off=ftell(fp);
		if(real_off < off){
			/*For the lsb.events file, the first line has the format "# <history seek position>", 
			which indicates the file position of the first history event after log switch.*/ 
			off=GetHistorySeekPos(fp);
		}
		
		if(fseek(fp, off, SEEK_SET) < 0){
			sysfatal("couldn't seek: %r");
		}
        
		tmp_off = tail(fp, line, off);
		off=tmp_off;
		fclose(fp);
		sleep(1);
	}        
}

long
tail(FILE *fp, char *line, long old_off)
{
	long act_off=old_off;

	while(fgets(line, MAX_CHARS, fp)){
		if (strrchr(line, '\n') == NULL){
			return act_off;
		}
		if(line && ((strstr(line,rex_queued)!=NULL) || (strstr(line,rex_running)!=NULL) || (strstr(line,rex_status)!=NULL) || (strstr(line,rex_signal)!=NULL))){        
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

        char *s;

	if(!value || (strlen(value)==0) || (strcmp(value,"\n")==0)){
		return -1;
	}

	do_log(debuglogfile, debug, 1, "Adding: ID:%d Type:%s Value:%s\n",rptr[id],flag,value);
	/* set write lock */
	pthread_mutex_lock( &write_mutex );
 
	if((strcmp(flag,"JOBID")==0) && j2js[id] == NULL){
  
		j2js[id] = strdup("1");  
		j2bl[id] = strdup("\0");
		j2wn[id] = strdup("\0");
		j2ec[id] = strdup("\0");
		j2st[id] = strdup("\0");
		j2rt[id] = strdup("\0");
		j2ct[id] = strdup("\0");
		
		reccnt[id] = recycled;
  
	} else if((strcmp(flag,"JOBID")==0) && recycled>0){
 
		free(j2js[id]);
		free(j2bl[id]);
		free(j2wn[id]);
		free(j2ec[id]);
		free(j2st[id]);
		free(j2rt[id]);
		free(j2ct[id]);
  
		j2js[id] = strdup("1");  
		j2bl[id] = strdup("\0");
		j2wn[id] = strdup("\0");
		j2ec[id] = strdup("\0");
		j2st[id] = strdup("\0");
		j2rt[id] = strdup("\0");
		j2ct[id] = strdup("\0");
		
		reccnt[id] = recycled;
    
	} else if(strcmp(flag,"BLAHPNAME")==0){
 
		free(j2bl[id]);
		s=strdel(value,"\"");
		j2bl[id] = strdup(s);
		free(s);
  
	} else if(strcmp(flag,"WN")==0){
 
		free(j2wn[id]);
		j2wn[id] = strdup(value);
  
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

int AddToStruct(char *line, int flag){

	/*if flag ==0 AddToStruct is called within GetOldLogs 
	if flag ==1 AddToStruct is called elsewhere*/

	int has_blah=0;
	char *rex;
 
	int id,realid;
	int belongs_to_current_cycle;
 
	int  maxtok; 
	char **tbuf;
 
	char *jobid=NULL;
	
	char *tj_time=NULL;
	char *j_time=NULL;
	char *tmptime=NULL;
	
	char *j_status=NULL;
	char *j_reason=NULL;
 
	char *ex_status=NULL;
	char *failex_status=NULL;
 
	char *sig_status=NULL;
	char *j_blahjob=NULL;
	char *wnode=NULL;
 
	maxtok=strtoken(line,' ',&tbuf);
 
	if(maxtok>0){
		rex=strdup(tbuf[0]);
	}
	if(maxtok>2){
		tj_time=strdup(tbuf[2]);
		j_time=epoch2str(tj_time);
		tmptime=strdup(j_time);
	}
	if(maxtok>3){
		jobid=strdup(tbuf[3]);
		realid=atoi(jobid);
	}
	if(maxtok>4){
		j_status=strdup(tbuf[4]);
	}
	if(maxtok>5){
		j_reason=strdup(tbuf[5]);
	}
	if(maxtok>6){
		sig_status=strdup(tbuf[6]);
	}
	if(maxtok>9){
		wnode=strdup(tbuf[9]);
	}
	if(maxtok>10){
		ex_status=strdup(tbuf[10]);
	}
	if(maxtok>29){
		failex_status=strdup(tbuf[29]);
	}
	if(maxtok>41){
		j_blahjob=strdup(tbuf[41]);
		if(j_blahjob && ((strstr(j_blahjob,blahjob_string)!=NULL) || (strstr(j_blahjob,bl_string)!=NULL) || (strstr(j_blahjob,cream_string)!=NULL))){
			has_blah=1;
		}
	}
	
	freetoken(&tbuf,maxtok);

	id=UpdatePtr(realid,rex,has_blah);
	
	belongs_to_current_cycle = 0;
	if((id >= 0) && ((reccnt[id]==recycled) || 
	   ((id >= ptrcnt) && (reccnt[id]==(recycled-1)))))
		belongs_to_current_cycle = 1;
 
	if((id >= 0) && rex && (strcmp(rex,rex_queued)==0) && (has_blah)){

		InfoAdd(id,jobid,"JOBID");
		InfoAdd(id,j_time,"STARTTIME");
		InfoAdd(id,j_blahjob,"BLAHPNAME");
 
		if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
			NotifyCream(id, "1", j2bl[id], "NA", "NA", j2st[id], flag);
		}
  
	} else if((id >= 0) && (belongs_to_current_cycle) && (j2bl[id]) && ((strstr(j2bl[id],blahjob_string)!=NULL) || (strstr(j2bl[id],bl_string)!=NULL) || (strstr(j2bl[id],cream_string)!=NULL))){ 

		if(rex && strcmp(rex,rex_running)==0){

			InfoAdd(id,"2","JOBSTATUS");
			InfoAdd(id,wnode,"WN");
			InfoAdd(id,j_time,"RUNNINGTIME");
   
			if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
				NotifyCream(id, "2", j2bl[id], j2wn[id], "NA", j2rt[id], flag);
			}
  
		} else if(rex && strcmp(rex,rex_signal)==0){
  
			if(sig_status && strstr(sig_status,"KILL")!=NULL){

				InfoAdd(id,"3","JOBSTATUS");

				if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					if(j2wn[id]!=NULL){
						NotifyCream(id, "3", j2bl[id], j2wn[id], "NA", tmptime, flag);
					}else{
						NotifyCream(id, "3", j2bl[id], "NA", "NA", tmptime, flag);
					}
				}
			}
     
		} else if(rex && strcmp(rex,rex_status)==0){
    
			if(j_status && strcmp(j_status,"192")==0){

				if(j2js[id] && strcmp(j2js[id],"3")!=0){
					InfoAdd(id,"4","JOBSTATUS");
					InfoAdd(id,"0","EXITCODE");
					InfoAdd(id,j_time,"COMPLTIME");
    
					if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
						NotifyCream(id, "4", j2bl[id], j2wn[id], j_reason, j2ct[id], flag);
					}
				}

			}  else if(j_status && strcmp(j_status,"320")==0){
			
				if(j2js[id] && strcmp(j2js[id],"3")!=0){
					InfoAdd(id,"4","JOBSTATUS");
					InfoAdd(id,"-1","EXITCODE");
					InfoAdd(id,j_time,"COMPLTIME");
    
					if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
						NotifyCream(id, "4", j2bl[id], j2wn[id], "-1", j2ct[id], flag);
					}
				}
			
			}  else if(j_status && strcmp(j_status,"32")==0){

				if(j2js[id] && strcmp(j2js[id],"3")!=0){
					InfoAdd(id,"4","JOBSTATUS");
					if(failex_status!=NULL){
						InfoAdd(id,failex_status,"EXITCODE");
					}else{
						InfoAdd(id,j_reason,"EXITCODE");
					}
					InfoAdd(id,j_time,"COMPLTIME");
					if(failex_status!=NULL){
						if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
							NotifyCream(id, "4", j2bl[id], j2wn[id], failex_status, j2ct[id], flag);
						}
					}else{
						if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
							NotifyCream(id, "4", j2bl[id], j2wn[id], j_reason, j2ct[id], flag);
						}
					}
					
				}

			} else if((j_status && strcmp(j_status,"16")==0) || (j_status && strcmp(j_status,"8")==0) || (j_status && strcmp(j_status,"2")==0)){

				InfoAdd(id,"5","JOBSTATUS");
    
				if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					if(j2wn[id]!=NULL){
						NotifyCream(id, "5", j2bl[id], j2wn[id], j_reason, tmptime, flag);
					}else{
						NotifyCream(id, "5", j2bl[id], "NA", j_reason, tmptime, flag);
					}
				}

			} else if(j_status && strcmp(j_status,"4")==0){

				InfoAdd(id,"2","JOBSTATUS");
    
				if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					NotifyCream(id, "2", j2bl[id], j2wn[id], j_reason, tmptime, flag);
				}

			} else if(j_status && strcmp(j_status,"1")==0){

				InfoAdd(id,"1","JOBSTATUS");
    
				if((usecream>0) && j2bl[id] && (strstr(j2bl[id],cream_string)!=NULL)){
					NotifyCream(id, "1", j2bl[id], "NA", j_reason, tmptime, flag);
				}

			}
		} /* closes if-else if on rex_ */
	} /* closes if-else if on jobid lookup */
 
	free(rex);
	free(j_time);
	free(tj_time);
	free(tmptime);
	free(jobid);
	free(j_status);
	free(sig_status);
	free(wnode);
	free(ex_status);
	free(failex_status);
	free(j_blahjob);
	free(j_reason);

	return 0;
}

char *
GetAllEvents(char *file)
{
 
	FILE *fp;
	char *line;
	char **opfile;
	int maxtok,i;
	char *full_logname=NULL;
 
	maxtok = strtoken(file, ' ', &opfile);

	if((line=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc line: %r");
	}
  
	for(i=0; i<maxtok; i++){ 
 
		full_logname=make_message("%s/%s",ldir,opfile[i]);
		if((fp=fopen(full_logname, "r")) != 0){
			while(fgets(line, STR_CHARS, fp)){
				if(line && ((strstr(line,rex_queued)!=NULL) || (strstr(line,rex_running)!=NULL) || (strstr(line,rex_status)!=NULL) || (strstr(line,rex_signal)!=NULL))){
					AddToStruct(line,0);
				}
			}
			fclose(fp);
		} else {
			syserror("Cannot open %s file: %r",opfile[i]);
		}
		free(full_logname);
  
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
	char      *t_wnode=NULL;
	char      *exitreason=NULL;
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
			sysfatal("Error calling accept() in LookupAndSend: %r");
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
       
		if(strcmp(logdate,"HELP")==0){
			out_buf=make_message("Commands: BLAHJOB/<blahjob-id> <date-YYYYmmdd>/<jobid> HELP TEST VERSION CREAMPORT TOTAL LISTALL LISTF[/<first-n-jobid>] LISTL[/<last-n-jobid>]\n");
			goto close;
		}

/* TEST reply */
       
		if(strcmp(logdate,"TEST")==0){
			out_buf=make_message("YLSF\n");
			goto close;
		}

/* VERSION reply */
       
		if(strcmp(logdate,"VERSION")==0){
			out_buf=make_message("%s\n",VERSION);
			goto close;
		}
	
/* TOTAL reply */
       
		if(strcmp(logdate,"TOTAL")==0){
			if(recycled){
				out_buf=make_message("Total number of jobs:%d\n",RDXHASHSIZE);
			}else{
				out_buf=make_message("Total number of jobs:%d\n",ptrcnt-1);
			}
			goto close;
		}
	
/* LISTALL reply */

		if(strcmp(logdate,"LISTALL")==0){
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
       
		if(strcmp(logdate,"LISTF")==0){
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
       
		if(strcmp(logdate,"LISTL")==0){
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
       
		if(strcmp(logdate,"CREAMPORT")==0){
			out_buf=make_message("%d\n",creamport);
			goto close;
		}

/* get jobid from blahjob id (needed by *_submit.sh) */
	
		if(strcmp(logdate,"BLAHJOB")==0){
			for(i=0;i<WRETRIES;i++){
				pthread_mutex_lock(&write_mutex);
				bid=GetBlahNameId(jobid);
				if(bid==-1){
					pthread_mutex_unlock(&write_mutex);
					sleep(1);
					continue;
				}
				out_buf=make_message("%d\n",rptr[bid]);
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
         
			if(j2wn[id] && strcmp(j2wn[id],"\0")==0){
				t_wnode=make_message("");
			}else{
				t_wnode=make_message("WorkerNode=%s;",j2wn[id]);
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
			if(j2js[id] && strcmp(j2js[id],"4")==0){
				if(j2ec[id] && ((strcmp(j2ec[id],"130")==0) || (strcmp(j2ec[id],"137")==0) || (strcmp(j2ec[id],"143")==0))){
					exitreason=make_message(" ExitReason=\"Memory limit reached\";");
				}else if(j2ec[id] && strcmp(j2ec[id],"140")==0){
					exitreason=make_message(" ExitReason=\"RUNtime limit reached\";");
				}else if(j2ec[id] && strcmp(j2ec[id],"152")==0){
					exitreason=make_message(" ExitReason=\"CPUtime limit reached\";");
				}else if(j2ec[id] && strcmp(j2ec[id],"153")==0){
					exitreason=make_message(" ExitReason=\"FILEsize limit reached\";");
				}else if(j2ec[id] && strcmp(j2ec[id],"157")==0){
					exitreason=make_message(" ExitReason=\"Directory Access Error (No AFS token, dir does not exist)\";");
				}else{
					exitreason=make_message("");
				}
				out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\"; LRMSCompletedTime=\"%s\";%s JwExitCode=%s;]/%s\n",jobid, t_wnode, j2js[id], j2st[id], j2rt[id], j2ct[id], exitreason, j2ec[id], pr_removal);
			}else if(j2rt[id] && strcmp(j2rt[id],"\0")!=0){
				out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\";]/%s\n",jobid, t_wnode, j2js[id], j2st[id], j2rt[id], pr_removal);
			}else{
				out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\";]/%s\n",jobid, t_wnode, j2js[id], j2st[id], pr_removal);
			}
			pthread_mutex_unlock(&write_mutex);

			free(t_wnode);
			free(exitreason);
		} else {
        
			pthread_mutex_unlock(&write_mutex);
			GetEventsInOldLogs(logdate);
         
			id=GetRdxId(atoi(jobid));

			pthread_mutex_lock(&write_mutex);
			if(id>0 && j2js[id]!=NULL){

				if(j2wn[id] && strcmp(j2wn[id],"\0")==0){
					t_wnode[0]='\0';
				}else{
					t_wnode=make_message("WorkerNode=%s;",j2wn[id]);
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
				if(j2js[id] && strcmp(j2js[id],"4")==0){
					if(j2ec[id] && ((strcmp(j2ec[id],"130")==0) || (strcmp(j2ec[id],"137")==0) || (strcmp(j2ec[id],"143")==0))){
						exitreason=make_message(" ExitReason=\"Memory limit reached\";");
					}else if(j2ec[id] && strcmp(j2ec[id],"140")==0){
						exitreason=make_message(" ExitReason=\"RUNtime limit reached\";");
					}else if(j2ec[id] && strcmp(j2ec[id],"152")==0){
						exitreason=make_message(" ExitReason=\"CPUtime limit reached\";");
					}else if(j2ec[id] && strcmp(j2ec[id],"153")==0){
						exitreason=make_message(" ExitReason=\"FILEsize limit reached\";");
					}else if(j2ec[id] && strcmp(j2ec[id],"157")==0){
						exitreason=make_message(" ExitReason=\"Directory Access Error (No AFS token, dir does not exist)\";");
					}else{
						exitreason=make_message("");
					}
					out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\"; LRMSCompletedTime=\"%s\";%s JwExitCode=%s;]/%s\n",jobid, t_wnode, j2js[id], j2st[id], j2rt[id], j2ct[id], exitreason, j2ec[id], pr_removal);
				}else if(j2rt[id] && strcmp(j2rt[id],"\0")!=0){
					out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\"; LRMSStartRunningTime=\"%s\";]/%s\n",jobid, t_wnode, j2js[id], j2st[id], j2rt[id], pr_removal);
				}else{
					out_buf=make_message("[BatchJobId=\"%s\"; %s JobStatus=%s; LRMSSubmissionTime=\"%s\";]/%s\n",jobid, t_wnode, j2js[id], j2st[id], pr_removal);
				}
				free(t_wnode);
				free(exitreason);
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
GetLogDir(int largc, char *largv[])
{

	char *lsf_base_pathtmp;
	char *lsf_base_path;
	char *conffile;
	char *lsf_clustername;
	char *logpath;
	char *line;
	char *command_string;
	int len;
	int version=0;
	int rc;
	int c;
 
	struct stat  sbuf;
	
	static int help;
	static int short_help;    
 
	char *ebinpath;
	char *econfpath;

	int  maxtok; 
	char **tbuf;
	char *cp;
	char *s;
  
	FILE *fp;
	FILE *file_output;
    

	if((line=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc line: %r");
	}
	if((lsf_clustername=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc lsf_clustername: %r");
	}

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
		{"confpath",  required_argument,       0, 'c'},
		{"logfile",   required_argument,       0, 'l'},
		{"debug",     required_argument,       0, 'd'},
		{0, 0, 0, 0}
		};

		int option_index = 0;
     
		c = getopt_long (largc, largv, "Dvp:m:b:c:l:d:",long_options, &option_index);
     
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
	       
		case 'c':
			confpath=strdup(optarg);
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
	if(port) {
		if ( port < 1 || port > 65535) {
			sysfatal("Invalid port supplied: %r");
		}
	}else{
		port=DEFAULT_PORT;
	}   

	if ( creamport > 0 && port <= 65535 ) {
		usecream=1;
	}

	if(debug <=0){
		debug=0;
	}
	
	if(debug){
		if((debuglogfile = fopen(debuglogname, "a+"))==0){
			debuglogfile =  fopen("/dev/null", "a+");
		}
	}
  
	if((econfpath=getenv("LSF_ENVDIR"))!=NULL){
		conffile=make_message("%s/lsf.conf",econfpath);
		if((fp=fopen(conffile, "r")) != 0){
			while(fgets(line, STR_CHARS, fp)){
				if(line && strstr(line,"LSB_SHAREDIR")!=0){
					goto creamdone;
				}
			}
		}
	}
	
	conffile=make_message("%s/lsf.conf",confpath);
	
	if((fp=fopen(conffile, "r")) != 0){
		while(fgets(line, STR_CHARS, fp)){
			if(line && strstr(line,"LSB_SHAREDIR")!=0){
				goto creamdone;
			}
		}
	}
	
	if((econfpath=getenv("LSF_CONF_PATH"))!=NULL){
		conffile=make_message("%s/lsf.conf",econfpath);
		if((fp=fopen(conffile, "r")) != 0){
			while(fgets(line, STR_CHARS, fp)){
				if(line && strstr(line,"LSB_SHAREDIR")!=0){
					goto creamdone;
				}
			}
		}
	}

creamdone:
	maxtok=strtoken(line,'=',&tbuf);
	if(tbuf[1]){
		lsf_base_pathtmp=strdup(tbuf[1]);
	} else {
		do_log(debuglogfile, debug, 1, "Unable to find logdir in conf file");
		sysfatal("Unable to find logdir in conf file: %r");
	}
 
	if ((cp = strrchr (lsf_base_pathtmp, '\n')) != NULL){
		*cp = '\0';
	}
 
	lsf_base_path=strdel(lsf_base_pathtmp, "\" ");
	free(lsf_base_pathtmp);
 
	if((ebinpath=getenv("LSF_BINDIR"))!=NULL){
 
		s=make_message("%s/lsid",ebinpath);
		rc=stat(s,&sbuf);
		if(rc) {
			do_log(debuglogfile, debug, 1, "%s not found",s);
			sysfatal("%s not found: %r",s);
		}
		if( ! (sbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) ) {
			do_log(debuglogfile, debug, 1, "%s is not executable, but mode %05o",s,(int)sbuf.st_mode);
			sysfatal("%s is not executable, but mode %05o: %r",s,(int)sbuf.st_mode);
		}
		free(s);
		command_string=make_message("%s/lsid | grep 'My cluster name is'|awk -F\" \" '{ print $5 }'",ebinpath); 
		goto bdone;
	}
 
	s=make_message("%s/lsid",binpath);
	rc=stat(s,&sbuf);
	if(rc) {
		do_log(debuglogfile, debug, 1, "%s not found",s);
		sysfatal("%s not found: %r",s);
	}
	if( ! (sbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) ) {
		do_log(debuglogfile, debug, 1, "%s is not executable, but mode %05o",s,(int)sbuf.st_mode);
		sysfatal("%s is not executable, but mode %05o: %r",s,(int)sbuf.st_mode);
	}
	free(s);
	command_string=make_message("%s/lsid | grep 'My cluster name is'|awk -F\" \" '{ print $5 }'",binpath);
	goto bdone;
 
	if((ebinpath=getenv("LSF_BIN_PATH"))!=NULL){

		s=make_message("%s/lsid",ebinpath);
		rc=stat(s,&sbuf);
		if(rc) {
			do_log(debuglogfile, debug, 1, "%s not found",s);
			sysfatal("%s not found: %r",s);
		}
		if( ! (sbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) ) {
			do_log(debuglogfile, debug, 1, "%s is not executable, but mode %05o",s,(int)sbuf.st_mode);
			sysfatal("%s is not executable, but mode %05o: %r",s,(int)sbuf.st_mode);
		}
		free(s);
		command_string=make_message("%s/lsid | grep 'My cluster name is'|awk -F\" \" '{ print $5 }'",ebinpath);  
		goto bdone;
	}
 
bdone:
	file_output = popen(command_string,"r");
 
	if (file_output != NULL){
		len = fread(lsf_clustername, sizeof(char), STR_CHARS - 1 , file_output);
		if (len>0){
			lsf_clustername[len-1]='\000';
		}else{
			sleep(1);
			pclose(file_output);
			goto bdone;
		}
	}
	pclose(file_output);
 
	logpath=make_message("%s/%s/logdir",lsf_base_path,lsf_clustername);
 
        freetoken(&tbuf,maxtok);
	
	free(line);
	free(lsf_base_path);
	free(lsf_clustername);
	free(conffile);
	free(command_string);
 
	return logpath;

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
	int 		n;

	if((slogs=calloc(MAX_CHARS*4,1)) == 0){
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

	n = scandir(ldir, &direntry, 0, versionsort);
	if (n < 0){
		syserror("scandir error: %r");
		return NULL;
	} else {
		while(n--) {
			if( *(direntry[n]->d_name) == '.' ) continue;
			s=make_message("%s/%s",ldir,direntry[n]->d_name);
			rc=stat(s,&sbuf);
			if(rc) {
				syserror("Cannot stat file %s: %r", s);
				return NULL;
			}
			if ( sbuf.st_mtime > tage ) {
				if(strstr(direntry[n]->d_name,lsbevents)!=NULL && strstr(direntry[n]->d_name,"lock")==NULL && strstr(direntry[n]->d_name,"index")==NULL){
					strcat(slogs,direntry[n]->d_name);
					strcat(slogs," ");
				}
			}  
			free(s);
			free(direntry[n]);
		}
		free(direntry);
	}

	do_log(debuglogfile, debug, 1, "Log list:%s\n",slogs);
	 
	return(slogs);
}

void 
CreamConnection(int c_sock)
{ 

	char *buffer;
        char *buftmp;

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
	int   ii;
	char *notstr=NULL;
	char *notdate=NULL;
	char *lnotdate=NULL;
	int   notepoch;
	int   logepoch;

	int  maxtok,maxtok_s,maxtok_l,maxtok_b; 
	char **tbuf;
	char **sbuf;
	char **lbuf;
	char **bbuf;
	char *cp;
	char *fullblahstring=NULL;
	char *joblist_string=NULL;
	char *tjoblist_string=NULL;
	char *fullbljobid=NULL;
	char *tbljobid=NULL;

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
      
		if(cream_recycled){
			logepoch=nti[jcount];
		}else{
			logepoch=nti[0];
		} 
		if(logepoch<=0){
			logepoch=time(NULL);
		}     
		if(notepoch<=logepoch){
			lnotdate=iepoch2str(notepoch,"L");
			GetEventsInOldLogs(lnotdate);
			free(lnotdate);
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
            
		for(ii=0;ii<=jcount;ii++){
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
		tjoblist_string=strdup(sbuf[1]);
		
		joblist_string=make_message(",%s,",tjoblist_string);
		
		freetoken(&sbuf,maxtok_s);
		
		if(cream_recycled){
			logepoch=nti[jcount];
		}else{
			logepoch=nti[0];
		} 
		if(logepoch<=0){
			logepoch=time(NULL);
		}     
		if(notepoch<=logepoch){
			lnotdate=iepoch2str(notepoch,"L");
			GetEventsInOldLogs(lnotdate);
			free(lnotdate);
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
		outreason=make_message(" Reason=\"lsf_reason=%s\";" ,reason);
		if((strcmp(reason,"130")==0) || (strcmp(reason,"137")==0) || (strcmp(reason,"143")==0)){
			exitreason=make_message(" ExitReason=\"Memory limit reached\";");
		}else if(strcmp(reason,"140")==0){
			exitreason=make_message(" ExitReason=\"RUNtime limit reached\";");
		}else if(strcmp(reason,"152")==0){
			exitreason=make_message(" ExitReason=\"CPUtime limit reached\";");
		}else if(strcmp(reason,"153")==0){
			exitreason=make_message(" ExitReason=\"FILEsize limit reached\";");
		}else if(strcmp(reason,"157")==0){
			exitreason=make_message(" ExitReason=\"Directory Access Error (No AFS token, dir does not exist)\";");
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
UpdatePtr(int jid, char *rx, int has_bl)
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
		if(rx && (strcmp(rx,rex_queued)==0) && (has_bl)){
			rptr[ptrcnt++]=jid;
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
	printf("Usage: BLParserLSF [OPTION...]\n");
	printf("  -p, --port=<port number>               port\n");
	printf("  -m, --creamport=<creamport number>     creamport\n");
	printf("  -b, --binpath=<LSFbinpath>             LSF binpath\n");
	printf("  -c, --confpath=<LSFconfpath>           LSF confpath\n");
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
	printf("Usage: BLParserLSF [-Dv?] [-p|--port <port number>]\n");
	printf("        [-m|--creamport <creamport number>] [-b|--binpath <LSFbinpath>]\n");
	printf("        [-c|--confpath <LSFconfpath>] [-l|--logfile  <DebugLogFile>]\n");
	printf("        [-d|--debug INT] [-D|--daemon] [-v|--version] [-?|--help] [--usage]\n");
	exit(EXIT_SUCCESS);
}
