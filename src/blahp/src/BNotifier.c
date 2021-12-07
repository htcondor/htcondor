/*
#  File:     BNotifier.c
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

#include "BNotifier.h"

/* Variables initialization */

char *progname="BNotifier";

char *registry_file;

char *creamfilter="";

int async_notif_port;

int debug=FALSE;
int nodmn=FALSE;

FILE *debuglogfile;
char *debuglogname;

int  c_sock;

/* moved to per-thread structure
int startnotify=FALSE;
int startnotifyjob=FALSE;
int firstnotify=FALSE;
int sentendonce=FALSE;
char *joblist_string="";
time_t lastnotiftime;
 */
 
int loop_interval=DEFAULT_LOOP_INTERVAL;

creamConnection_t connections[MAX_CONNECTIONS] = { EMPTY_CONNECTION };

int
main(int argc, char *argv[])
{

	int set = 1;
	int status;
	int version = 0;
	int list_c;
	char ainfo_port_string[16];
	struct addrinfo ai_req, *ai_ans, *cur_ans;
	int address_found;
	int new_connection_fd;

	pthread_t PollThd;
	config_handle *cha;
	config_entry *ret;
	char *pidfile=NULL;
	int nocheck=0;
	
	int c;				

        static int help;
        static int short_help;
	
	int loop_interval=DEFAULT_LOOP_INTERVAL;

	while (1) {
		static struct option long_options[] =
		{
		{"help",      no_argument,     &help,       1},
		{"usage",     no_argument,     &short_help, 1},
		{"nocheck",   no_argument,       0, 'n'},
		{"nodaemon",  no_argument,       0, 'o'},
		{"version",   no_argument,       0, 'v'},
		{"prefix",    required_argument, 0, 'p'},
		{0, 0, 0, 0}
		};

		int option_index = 0;
     
		c = getopt_long (argc, argv, "vnop:",long_options, &option_index);
     
		if (c == -1){
			break;
		}
     
		switch (c)
		{

		case 0:
		if (long_options[option_index].flag != 0){
			break;
		}
     
		case 'v':
			version=1;
			break;
	       
		case 'n':
			nocheck=1;
			break;
			
		case 'o':
			nodmn=1;
			break;

		case 'p':
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

	argv0 = argv[0];
	
	/*Ignore sigpipe*/
    
	signal(SIGPIPE, SIG_IGN);             
        signal(SIGHUP,sighup);
 
	if(version) {
		printf("%s Version: %s\n",progname,VERSION);
		exit(EXIT_SUCCESS);
	}   

	/* Checking configuration */
	if(!nocheck){
		check_config_file("NOTIFIER");
	}

	/* Reading configuration */
	cha = config_read(NULL);
	if (cha == NULL)
	{
		fprintf(stderr,"Error reading config: ");
		perror("");
		exit(EXIT_FAILURE);
	}
	
	ret = config_get("bnotifier_debug_level",cha);
	if (ret != NULL){
		debug=atoi(ret->value);
	}
	
	ret = config_get("bnotifier_debug_logfile",cha);
	if (ret != NULL){
		debuglogname=strdup(ret->value);
                if(debuglogname == NULL){
                        sysfatal("strdup failed for debuglogname in main: %r");
                }
	}
	
	if(debug <=0){
		debug=0;
	}
    
	if(debuglogname){
		if((debuglogfile = fopen(debuglogname, "a+"))==0){
			debug = 0;
		}
	}else{
		debug = 0;
	}
        
	ret = config_get("job_registry",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key job_registry not found\n",argv0);
		sysfatal("job_registry not defined. Exiting");
	} else {
		registry_file=strdup(ret->value);
                if(registry_file == NULL){
                        sysfatal("strdup failed for registry_file in main: %r");
                }
	}
	
	ret = config_get("async_notification_port",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key async_notification_port not found\n",argv0);
	} else {
		async_notif_port =atoi(ret->value);
	}

	ret = config_get("bnotifier_loop_interval",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key bnotifier_loop_interval not found using the default:%d\n",argv0,loop_interval);
	} else {
		loop_interval=atoi(ret->value);
	}
	
	ret = config_get("bnotifier_pidfile",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key bnotifier_pidfile not found\n",argv0);
	} else {
		pidfile=strdup(ret->value);
                if(pidfile == NULL){
                        sysfatal("strdup failed for pidfile in main: %r");
                }
	}
	
	/* create listening socket for Cream */
    
	if ( !async_notif_port ) {
		sysfatal("Invalid port supplied for Cream: %r");
	}

	ai_req.ai_flags = AI_PASSIVE;
	ai_req.ai_family = PF_UNSPEC;
	ai_req.ai_socktype = SOCK_STREAM;
	ai_req.ai_protocol = 0; /* Any stream protocol is OK */

	sprintf(ainfo_port_string,"%5d",async_notif_port);

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

		if(setsockopt(list_c, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) 
		{
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

	if ( address_found == 0 ) {
		sysfatal("Error creating and binding socket: %r");
	}

	if ( listen(list_c, LISTENQ) < 0 ) {
		sysfatal("Error calling listen() in main: %r");
	}
    
	if( !nodmn ) daemonize();
	
	if( pidfile ){
		writepid(pidfile);
		free(pidfile);
	}
       
	config_free(cha);

	pthread_create(&PollThd, NULL, (void *(*)(void *))PollDB, (void *)NULL);

	for ( ;; ) {
		/* FIXME: exit condition??? */
		do_log(debuglogfile, debug, 1, "Listening for new connection\n");
		if ( (new_connection_fd = accept(list_c, NULL, NULL) ) < 0 ) 
		{
			do_log(debuglogfile, debug, 1, "Fatal Error:Error calling accept() on list_c\n");
			sysfatal("Error calling accept(): %r");
		}

		if (add_cream_connection(new_connection_fd) != 0)
		{
			do_log(debuglogfile, debug, 1, "connection table full: unable to add the new connection\n");
			close(new_connection_fd);
		}
	}

	pthread_join(PollThd, (void **)&status);
	pthread_exit(NULL);
 
}

/*---Functions---*/

int
PollDB()
{
        FILE *fd;
        job_registry_entry *en;
	job_registry_handle *rha;
	job_registry_handle *rhc;
	char *buffer=NULL;
	char *finalbuffer=NULL;
        char *cdate=NULL;
	time_t now;
        int  maxtok,i,maxtokl,j;
        char **tbuf;
        char **lbuf;
	int len=0,flen=0;
        struct stat sbuf;
        int rc;
	char *regfile;
        char *cp=NULL;
	int to_sleep=FALSE;
	int skip_reg_open=FALSE;
	
	rha=job_registry_init(registry_file, BY_BATCH_ID);
	if (rha == NULL){
		do_log(debuglogfile, debug, 1, "%s: Error initialising job registry %s\n",argv0,registry_file);
		fprintf(stderr,"%s: Error initialising job registry %s :",argv0,registry_file);
		perror("");
	}
	
	for(;;){
	
		now=time(NULL);
	
		to_sleep=TRUE;
		/* cycle over connections: sleep if startnotify, startnotifyjob and sentendonce are not set.
		   If startnotifyjob is set the conn is served.
		*/ 
		for(i=0; i<MAX_CONNECTIONS; i++){
		
			if(!connections[i].startnotify && !connections[i].startnotifyjob && !(connections[i].firstnotify && connections[i].sentendonce)) continue;
			if(connections[i].startnotify) to_sleep=FALSE;
			
			if(connections[i].startnotifyjob){
				to_sleep=FALSE;
				rhc=job_registry_init(registry_file, BY_USER_PREFIX);
				if (rhc == NULL){
					do_log(debuglogfile, debug, 1, "%s: Error initialising job registry %s\n",argv0,registry_file);
					fprintf(stderr,"%s: Error initialising job registry %s :",argv0,registry_file);
		 	   	  	perror("");
		 	   	}
		 	   	do_log(debuglogfile, debug, 2, "%s:Job list for notification:%s\n",argv0,connections[i].joblist_string);
		 	   	maxtok=strtoken(connections[i].joblist_string,',',&tbuf);
   		 	   	for(j=0;j<maxtok;j++){
        	 	   	  	if ((en=job_registry_get(rhc, tbuf[j])) != NULL){
						buffer=ComposeClassad(en);
		 	   	  	}else{
		 	   	  		cdate=iepoch2str(now);
		 	   	  		maxtokl=strtoken(tbuf[j],'_',&lbuf);
		 	   	  		if(lbuf[1]){
		 	   	  			if ((cp = strrchr (lbuf[1], '\n')) != NULL){
		 	   	  				*cp = '\0';
		 	   	  			}
		 	   	  			if ((cp = strrchr (lbuf[1], '\r')) != NULL){
		 	   	  				*cp = '\0';
		 	   	  			}
		 	   	  			buffer=make_message("[BlahJobName=\"%s\"; ClientJobId=\"%s\"; JobStatus=4; JwExitCode=999; ExitReason=\"BUpdater is not able to find the job anymore\"; Reason=\"BUpdater is not able to find the job anymore\"; ChangeTime=\"%s\"; ]\n",tbuf[j],lbuf[1],cdate);
		 	   	  		}
		 	   	  		freetoken(&lbuf,maxtokl);
		 	   	  		free(cdate);
		 	   	  	}
		 	   	  	free(en);
		 	   	  	len=strlen(buffer);
		 	   	  	if(connections[i].finalbuffer != NULL){
		 	   	  		flen=strlen(connections[i].finalbuffer);
		 	   	  	}else{
		 	   	  		flen=0;
		 	   	  	}
		 	   	  	connections[i].finalbuffer = realloc(connections[i].finalbuffer,flen+len+2);
		 	   	  	if (connections[i].finalbuffer == NULL){
		 	   	  		sysfatal("can't realloc finalbuffer in PollDB: %r");
		 	   	  	}
		 	   	  	if(flen==0){
		 	   	  		connections[i].finalbuffer[0]='\000';
					}
		 	   	  	strcat(connections[i].finalbuffer,buffer);
		 	   	  	free(buffer);
		 	   	}
		 	   	freetoken(&tbuf,maxtok);
		 	   
		 	   	if(connections[i].finalbuffer != NULL){
		 	   	  	if(NotifyCream(connections[i].finalbuffer,&connections[i])!=-1){
	         	   	  		/* change last notification time */
		 	   	  		connections[i].lastnotiftime=now;
		 	   	  		connections[i].startnotifyjob=FALSE;
		 	   	  	}
		 	   	  	free(connections[i].finalbuffer);
		 	   	  	connections[i].finalbuffer=NULL;
		 	   	}
		 	   	job_registry_destroy(rhc);
			}
			if(connections[i].firstnotify && connections[i].sentendonce){
				to_sleep=FALSE;
				if(NotifyCream("NTFDATE/END\n",&connections[i])!=-1){
					connections[i].startnotify=TRUE;
					connections[i].sentendonce=FALSE;
		 	   	  	connections[i].firstnotify=FALSE;
		 	   	  	connections[i].startnotifyjob=FALSE;
				}
			}
			
		}
		
		if(to_sleep){
			sleep(loop_interval);
			continue;
		}

                regfile=make_message("%s/registry",registry_file);
        	rc=stat(regfile,&sbuf);
		free(regfile);
		
		skip_reg_open=TRUE;
		for(i=0; i<MAX_CONNECTIONS; i++){
			if(sbuf.st_mtime>=connections[i].lastnotiftime){
				skip_reg_open=FALSE;
				break;
			}
		}
		if(skip_reg_open){
			do_log(debuglogfile, debug, 3, "Skip registry opening: mtime:%d lastn:%d\n",sbuf.st_mtime,connections[i].lastnotiftime);
			sleep(loop_interval);
			continue;
		}
		
		do_log(debuglogfile, debug, 3, "Normal registry opening\n");

		fd = job_registry_open(rha, "r");
		if (fd == NULL)
		{
			do_log(debuglogfile, debug, 1, "%s: Error opening job registry %s\n",argv0,registry_file);
			fprintf(stderr,"%s: Error opening job registry %s :",argv0,registry_file);
			perror("");
			sleep(loop_interval);
			continue;
		}
		if (job_registry_rdlock(rha, fd) < 0)
		{
			do_log(debuglogfile, debug, 1, "%s: Error read locking registry %s\n",argv0,registry_file);
			fprintf(stderr,"%s: Error read locking registry %s :",argv0,registry_file);
			perror("");
			sleep(loop_interval);
			continue;
		}
		while ((en = job_registry_get_next(rha, fd)) != NULL)
		{
		
			for(i=0; i<MAX_CONNECTIONS; i++){
				if(connections[i].creamfilter==NULL) continue;
				if(en->mdate >= connections[i].lastnotiftime && en->mdate < now && en->user_prefix && strstr(en->user_prefix,connections[i].creamfilter)!=NULL && strlen(en->updater_info)>0)
				{
					buffer=ComposeClassad(en);
					len=strlen(buffer);
					if(connections[i].finalbuffer != NULL){
						flen=strlen(connections[i].finalbuffer);
					}else{
						flen=0;
					}
					connections[i].finalbuffer = realloc(connections[i].finalbuffer,flen+len+2);
					if (connections[i].finalbuffer == NULL){
						sysfatal("can't realloc finalbuffer in PollDB: %r");
					}
					if(flen==0){
						connections[i].finalbuffer[0]='\000';
					}
					strcat(connections[i].finalbuffer,buffer);
					free(buffer);
				}
			}
			free(en);
		}

		for(i=0; i<MAX_CONNECTIONS; i++){
			if(connections[i].finalbuffer != NULL){
				if(NotifyCream(connections[i].finalbuffer,&connections[i])!=-1){
	        			/* change last notification time */
					connections[i].lastnotiftime=now;
				}
				free(connections[i].finalbuffer);
				connections[i].finalbuffer=NULL;
			}
		}
		
		fclose(fd);
		
		sleep(loop_interval);
	}
                
	job_registry_destroy(rha);
	
	return 0;
}

char *
ComposeClassad(job_registry_entry *en)
{

	char *strudate=NULL;
	char *buffer=NULL;
	char *wn=NULL;
	char *excode=NULL;
	char *exreas=NULL;
	char *blahid=NULL;
	char *clientid=NULL;
        int  maxtok;
        char **tbuf;
	char *cp=NULL;
			
	if((buffer=calloc(STR_CHARS,1)) == 0){
		sysfatal("can't malloc buffer in PollDB: %r");
	}
		
	strudate=iepoch2str(en->udate);
	sprintf(buffer,"[BatchJobId=\"%s\"; JobStatus=%d; ChangeTime=\"%s\";",en->batch_id, en->status, strudate);
	free(strudate);

	if (strlen(en->wn_addr) > 0){
		wn=make_message(" WorkerNode=\"%s\";",en->wn_addr);
		strcat(buffer,wn);
		free(wn);
		}
	if (en->status == 3 || en->status == 4){
		excode=make_message(" JwExitCode=%d; Reason=\"reason=%d\";", en->exitcode, en->exitcode);
		strcat(buffer,excode);
		free(excode);
	}
	if (strlen(en->exitreason) > 0){
		exreas=make_message(" ExitReason=\"%s\";", en->exitreason);
		strcat(buffer,exreas);
		free(exreas);
	}
	if (strlen(en->user_prefix) > 0){
		maxtok=strtoken(en->user_prefix,'_',&tbuf);
		if(tbuf[1]){
			if ((cp = strrchr (tbuf[1], '\n')) != NULL){
				*cp = '\0';
			}
			if ((cp = strrchr (tbuf[1], '\r')) != NULL){
				*cp = '\0';
			}
			 clientid=make_message(" ClientJobId=\"%s\";",tbuf[1]);
		}
		blahid=make_message("%s BlahJobName=\"%s\";",clientid, en->user_prefix);
		strcat(buffer,blahid);
		free(blahid);
		freetoken(&tbuf,maxtok);
		free(clientid);
	}
	strcat(buffer,"]\n");
		
	return buffer;
		
}

void 
CreamConnection(creamConnection_t *connection)
{ 
/*
startnotify 	controls the normal operation in PollDB
startnotifyjob 	is used to send notification of jobs contained in joblist_string 
firstnotify 	controls if NTFDATE/END has to be sent (together with sentendonce)
sentendonce 	controls if NTFDATE/END has to be sent (is used to permit STARTNOTIFYJOBLIST to be used during
            	normal notifier operation without sending NTFDATE/END). It is reset to TRUE only by CREAMFILTER command
            	otherwise it remains FALSE after the first notification (finished with NTFDATE/END).
creamisconn 	starts all the normal usage: without it no notifications are sent to cream

So the initial commands should be:
CREAMFILTER
PARSERVERSION

STARTNOTIFY 
or 
STARTNOTIFYJOBLIST
STARTNOTIFYJOBEND

during the normal usage to have info about a list of job:
STARTNOTIFYJOBLIST
STARTNOTIFYJOBEND

*/

	char *buffer;
	int  conn_c = connection->socket_fd;

	if((buffer=calloc(LISTBUFFER,1)) == 0){
		sysfatal("can't malloc buffer in CreamConnection: %r");
	}

	while (Readline(conn_c, buffer, LISTBUFFER-1) > 0) {
		if(strlen(buffer) > 0)
		{
			do_log(debuglogfile, debug, 1, "Received for Cream:%s\n", buffer);
			if (strstr(buffer,"STARTNOTIFY/") != NULL) {
				NotifyStart(buffer, &(connection->lastnotiftime));
				connection->startnotify=TRUE;
				connection->firstnotify=TRUE;
			} else if (strstr(buffer,"STARTNOTIFYJOBLIST/") != NULL) {
				GetJobList(buffer, &(connection->joblist_string));
				connection->startnotifyjob = TRUE;
				connection->startnotify = FALSE;
			} else if (strstr(buffer,"STARTNOTIFYJOBEND/") != NULL) {
				connection->firstnotify=TRUE;
				connection->lastnotiftime = time(NULL);
			} else if (strstr(buffer,"CREAMFILTER/") != NULL) {
				GetFilter(buffer, connection->socket_fd, &(connection->creamfilter));
				connection->creamisconn=TRUE;
				connection->sentendonce=TRUE;
			} else if (strstr(buffer,"PARSERVERSION/") != NULL) {
				GetVersion(connection->socket_fd);
			}
		}
	}
	connection->creamisconn=FALSE;
	close(conn_c);
	free(buffer);
	connection->socket_fd = 0;
	if (connection->creamfilter) 
	{
		free(connection->creamfilter);
		connection->creamfilter = NULL;
	}
}

int 
GetVersion(const int conn_c)
{

	char *out_buf;

	if ((out_buf = make_message("%s__1\n", VERSION)) != NULL)
	{
		Writeline(conn_c, out_buf, strlen(out_buf));
		do_log(debuglogfile, debug, 1, "Sent Reply for PARSERVERSION command:%s\n",out_buf);
		free(out_buf);
		return 0;
	}
	else
		return 1;
}

int 
GetFilter(char *buffer, const int conn_c, char **creamfilter)
{

	int  maxtok;
	char **tbuf;
	char *cp=NULL;
	char * out_buf;

	maxtok = strtoken(buffer,'/',&tbuf);

	if(tbuf[1]){
		*creamfilter = make_message("%s",tbuf[1]);
		if(*creamfilter == NULL){
			sysfatal("strdup failed for creamfilter in GetFilter: %r");
		}
		if ((cp = strrchr (*creamfilter, '\n')) != NULL) {
			*cp = '\0';
		} 
		if ((cp = strrchr (*creamfilter, '\r')) != NULL) {
			*cp = '\0';
		}
		out_buf = make_message("CREAMFILTER set to %s\n", *creamfilter);

	} else {
		out_buf = make_message("CREAMFILTER ERROR\n");
	}
		
	Writeline(conn_c, out_buf, strlen(out_buf));

	do_log(debuglogfile, debug, 1, "Sent Reply for CREAMFILTER command:%s\n",out_buf);

	freetoken(&tbuf,maxtok);
	free(out_buf);
	
	return 0;
}

int 
NotifyStart(char *buffer, time_t *lastnotiftime)
{

	int  maxtok;
	char **tbuf;
	char *cp=NULL;
	char *notifdate=NULL;
	
	maxtok = strtoken(buffer,'/',&tbuf);

	if(tbuf[1]){
		notifdate=strdup(tbuf[1]);
		if(notifdate == NULL){
			sysfatal("strdup failed for notifdate in NotifyStart: %r");
		}
		if ((cp = strrchr (notifdate, '\n')) != NULL){
			*cp = '\0';
		}
		if ((cp = strrchr (notifdate, '\r')) != NULL){
			*cp = '\0';
		}
	}

	freetoken(&tbuf,maxtok);

	*lastnotiftime = str2epoch(notifdate,"S");
	free(notifdate);

	return 0;
}

int 
GetJobList(char *buffer, char **joblist_string)
{

	int  maxtok;
	char **tbuf;
	char *cp=NULL;

	maxtok=strtoken(buffer,'/',&tbuf);

	if(tbuf[1]){
		*joblist_string = strdup(tbuf[1]);
		if(*joblist_string == NULL){
			sysfatal("strdup failed for joblist_string in GetJobList: %r");
		}
		if ((cp = strrchr(*joblist_string, '\n')) != NULL) {
			*cp = '\0';
		} 
		if ((cp = strrchr(*joblist_string, '\r')) != NULL){
			*cp = '\0';
		}
	}

	freetoken(&tbuf,maxtok);

	return 0;
}


int
NotifyCream(char *buffer, creamConnection_t *connection)
{

	int      retcod;
	struct   pollfd fds[2];

	if (connection->creamfilter == NULL) return -1;
    
	fds[0].fd = connection->socket_fd;
	fds[0].events = ( POLLOUT | POLLPRI | POLLERR | POLLHUP | POLLNVAL ) ;
    
	if(!connection->creamisconn){
		return -1;
	}
	
	retcod = poll(fds, 1, bfunctions_poll_timeout); 
        
	if (retcod < 0) {
		free_cream_connection(connection);
		do_log(debuglogfile, debug, 1, "Fatal Error:Poll error in NotifyCream errno:%d\n",errno);
		sysfatal("Poll error in NotifyCream: %r");
	} else if ( retcod == 0 ) {
		do_log(debuglogfile, debug, 1, "Error:poll() timeout in NotifyCream\n");
		syserror("poll() timeout in NotifyCream: %r");
		return -1;
	} else if ( retcod > 0 ) {
		if (( fds[0].revents & ( POLLERR | POLLNVAL | POLLHUP) )){
			switch (fds[0].revents){
			case POLLNVAL:
				do_log(debuglogfile, debug, 1, "Error:poll() file descriptor error in NotifyCream\n");
				syserror("poll() file descriptor error in NotifyCream: %r");
				return -1;
			case POLLHUP:
				do_log(debuglogfile, debug, 1, "Connection closed in NotifyCream\n");
				syserror("Connection closed in NotifyCream: %r");
				return -1;
			case POLLERR:
				do_log(debuglogfile, debug, 1, "Error:poll() POLLERR in NotifyCream\n");
				syserror("poll() POLLERR in NotifyCream: %r");
				return -1;
			}
		} else {
			Writeline(connection->socket_fd, buffer, strlen(buffer));
			do_log(debuglogfile, debug, 1, "Sent for Cream:%s",buffer);
		} 
	}       

	return 0;

}

int
get_socket_by_creamprefix(const char* prefix)
{
	int i;

	for(i=0; i<MAX_CONNECTIONS; i++)
		if (connections[i].creamfilter != NULL && 
                    strcmp(prefix, connections[i].creamfilter) == 0)
			return connections[i].socket_fd;

	return 0; /* creamfilter not found */
}

int
add_cream_connection(const int socket_fd)
/* This must be invoked by the main thread only */
{
	int i;

	for(i=0; i<MAX_CONNECTIONS && connections[i].socket_fd != 0; i++);

	if (i<MAX_CONNECTIONS)
	{
		connections[i].socket_fd = socket_fd;
		pthread_create(&(connections[i].serving_thread), NULL, (void *(*)(void *))CreamConnection, (void *)(connections+i));
		return 0;
	}
	else
		return -1;
}

int
free_cream_connection(creamConnection_t *connection)
{
	if (connection->socket_fd) close(connection->socket_fd);
	if (connection->creamfilter) free (connection->creamfilter);
	if (connection->joblist_string) free (connection->joblist_string);

	connection->creamfilter = NULL;
	connection->socket_fd = 0;
	connection->lastnotiftime = 0;
	connection->serving_thread = 0;
	connection->startnotify = FALSE;
	connection->startnotifyjob = FALSE;
	connection->firstnotify = FALSE;
	connection->sentendonce = FALSE;
	connection->joblist_string = NULL;

	return 0;
}

void
sighup()
{
        if(debug){
                fclose(debuglogfile);
                if((debuglogfile = fopen(debuglogname, "a+"))==0){
                        debug = 0;
                }
        }
}

int 
usage()
{
	printf("Usage: BNotifier [OPTION...]\n");
	printf("  -o, --nodaemon     do not run as daemon\n");
	printf("  -v, --version      print version and exit\n");
	printf("\n");
	printf("Help options:\n");
	printf("  -?, --help         Show this help message\n");
	printf("  --usage            Display brief usage message\n");
	exit(EXIT_SUCCESS);
}

int 
short_usage()
{
	printf("Usage: BNotifier [-ov?] [-o|--nodaemon] [-v|--version] [-?|--help] [--usage]\n");
	exit(EXIT_SUCCESS);
}
