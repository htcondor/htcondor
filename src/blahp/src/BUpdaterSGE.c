/*
# Copyright (c) Members of the EGEE Collaboration. 2009.                                                                                                                                                             
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

#include "BUpdaterSGE.h"


extern int bfunctions_poll_timeout;

int main(int argc, char *argv[]){
    
    FILE *fd;
    job_registry_entry *en;
    time_t now;
    time_t purge_time=0;
    char *constraint[11];
    char *constraint2[5];
    char *query=NULL;
    char *queryStates=NULL;
    char *query_err=NULL;

    char *pidfile=NULL;
    char string_now[11];
    char *tpath;
    
    int version=0;
    int tmptim;
    int finstr_len=0;
    int loop_interval=DEFAULT_LOOP_INTERVAL;
    
    int fsq_ret=0;
    
    int c;
    
    int confirm_time=0;
    
    static int help;
    static int short_help;
    
    while (1) {
	static struct option long_options[] =
	{
	    {"help",      no_argument,     &help,       1},
	    {"usage",     no_argument,     &short_help, 1},
	    {"nodaemon",  no_argument,       0, 'o'},
	    {"version",   no_argument,       0, 'v'},
	    {0, 0, 0, 0}
	};
	
	int option_index = 0;
	
	c = getopt_long (argc, argv, "vo",long_options, &option_index);
	
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
		
	    case 'o':
		nodmn=1;
		break;
		
	    case '?':
		break;
		
	    default:
		abort ();
	}
    }
    
    //check if another instance is running 
    char **ptr;
    char out[3];
    fgets(out, sizeof(out),popen("ps -d | grep -c BUpdaterSGE","r"));
    strtoken(out,'\n',&ptr);
    if (strcmp(ptr[0],"1")!=0){
	fprintf(stderr,"There is another instance of BUpdaterSGE running.\nExiting ...\n");
	return -1;
    }
    freetoken(&ptr,1);

    if(help){
	usage();
    }
    
    if(short_help){
	short_usage();
    }
    
    argv0 = argv[0];
    
    signal(SIGHUP,sighup);
    
    if(version) {
	printf("%s Version: %s\n",progname,VERSION);
	exit(EXIT_SUCCESS);
    }  
    
    /* Checking configuration */
    check_config_file("UPDATER"); 
    
    cha = config_read(NULL);
    if (cha == NULL)
    {
	fprintf(stderr,"Error reading config: ");
	perror("");
	return -1;
    }
    
    ret = config_get("bupdater_child_poll_timeout",cha);
    if (ret != NULL){
	tmptim=atoi(ret->value);
	if (tmptim > 0) bfunctions_poll_timeout = tmptim*1000;
    }
    
    ret = config_get("bupdater_debug_level",cha);
    if (ret != NULL){
	debug=atoi(ret->value);
    }
    
    ret = config_get("bupdater_debug_logfile",cha);
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

    ret = config_get("debug_level",cha);
    if (ret != NULL){
	debug=atoi(ret->value);
    }

    ret = config_get("debug_logfile",cha);
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

    ret = config_get("sge_binpath",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key sge_binpath not found\n",argv0);
    } else {
	sge_binpath=strdup(ret->value);
	if(sge_binpath == NULL){
	    sysfatal("strdup failed for sge_binpath in main: %r");
	}
    }

    ret = config_get("sge_rootpath",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key sge_rootpath not found\n",argv0);
    } else {
	sge_rootpath=strdup(ret->value);
	if(sge_rootpath == NULL){
	    sysfatal("strdup failed for sge_rootpath in main: %r");
	}
	
	tpath=make_message("%s",sge_rootpath);
	if (opendir(tpath)==NULL){
	    do_log(debuglogfile, debug, 1, "%s: dir %s does not exist or is not readable\n",argv0,tpath);
	    sysfatal("dir %s does not exist or is not readable: %r",tpath);
	}
	free(tpath);
    }

    ret = config_get("sge_cellname",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key sge_cellname not found\n",argv0);
    } else {
	sge_cellname=strdup(ret->value);
	if(sge_cellname == NULL){
	    sysfatal("strdup failed for sge_cellname in main: %r");
	}
    }

    ret = config_get("sge_rootpath",cha);
    if (ret == NULL){
	if(debug){
	    fprintf(debuglogfile, "%s: key sge_rootpath not found\n",argv0);
	    fflush(debuglogfile);
	}
    } else {
	sge_rootpath=strdup(ret->value);
	if(sge_rootpath == NULL){
	    sysfatal("strdup failed for sge_rootpath in main: %r");
	}
    }

    ret = config_get("sge_cellname",cha);
    if (ret == NULL){
	if(debug){
	    fprintf(debuglogfile, "%s: key sge_cellname not found\n",argv0);
	    fflush(debuglogfile);
	}
    } else {
	sge_cellname=strdup(ret->value);
	if(sge_cellname == NULL){
	    sysfatal("strdup failed for sge_cellname in main: %r");
	}
    }

    ret = config_get("job_registry",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key job_registry not found\n",argv0);
	sysfatal("job_registry not defined. Exiting");
    } else {
	reg_file=strdup(ret->value);
	if(reg_file == NULL){
	    sysfatal("strdup failed for reg_file in main: %r");
	}
    }

    ret = config_get("purge_interval",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key purge_interval not found using the default:%d\n",argv0,purge_interval);
    } else {
	purge_interval=atoi(ret->value);
    }

    ret = config_get("finalstate_query_interval",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key finalstate_query_interval not found using the default:%d\n",argv0,finalstate_query_interval);
    } else {
	finalstate_query_interval=atoi(ret->value);
    }

    ret = config_get("alldone_interval",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key alldone_interval not found using the default:%d\n",argv0,alldone_interval);
    } else {
	alldone_interval=atoi(ret->value);
    }

    ret = config_get("bupdater_loop_interval",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key bupdater_loop_interval not found using the default:%d\n",argv0,loop_interval);
    } else {
	loop_interval=atoi(ret->value);
    }

    ret = config_get("bupdater_pidfile",cha);
    if (ret == NULL){
	do_log(debuglogfile, debug, 1, "%s: key bupdater_pidfile not found\n",argv0);
    } else {
	pidfile=strdup(ret->value);
	if(pidfile == NULL){
	    sysfatal("strdup failed for pidfile in main: %r");
	}
    }

    ret = config_get("job_registry_use_mmap",cha);
    if (ret == NULL){
        do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap not found. Default is NO\n",argv0);
    } else {
        do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap is set to %s\n",argv0,ret->value);
    }

    if( !nodmn ) daemonize();

    if( pidfile ){
	writepid(pidfile);
	free(pidfile);
    }

    config_free(cha);
    rha=job_registry_init(reg_file, BY_BATCH_ID);
    if (rha == NULL){
	do_log(debuglogfile, debug, 1, "%s: Error initialising job registry %s\n",argv0,reg_file);
	fprintf(stderr,"%s: Error initialising job registry %s :",argv0,reg_file);
	perror("");
    }
   for(;;){
	/* Purge old entries from registry */
	now=time(0);
	if(now - purge_time > 86400){
	    if(job_registry_purge(reg_file, now-purge_interval,0)<0){
		do_log(debuglogfile, debug, 1, "%s: Error purging job registry %s\n",argv0,reg_file);
		fprintf(stderr,"%s: Error purging job registry %s :",argv0,reg_file);
		perror("");
	    }else{
		purge_time=time(0);
	    }
	}
	
	//IntStateQuery();
	fd = job_registry_open(rha, "r");
	if (fd == NULL)
	{
	    do_log(debuglogfile, debug, 1, "%s: Error opening job registry %s\n",argv0,reg_file);
	    fprintf(stderr,"%s: Error opening job registry %s :",argv0,reg_file);
	    perror("");
	    sleep(loop_interval);
	}
	if (job_registry_rdlock(rha, fd) < 0)
	{
	    do_log(debuglogfile, debug, 1, "%s: Error read locking job registry %s\n",argv0,reg_file);
	    fprintf(stderr,"%s: Error read locking job registry %s :",argv0,reg_file);
	    perror("");
	    sleep(loop_interval);
	}
	job_registry_firstrec(rha,fd);
	fseek(fd,0L,SEEK_SET);

	if((query=calloc(STR_CHARS*2,1)) == 0){
	    sysfatal("can't malloc query %r");
	}
	if((queryStates=calloc(STR_CHARS*2,1)) == 0){
	    sysfatal("can't malloc query %r");
	}
	
	query[0]=' ';
	queryStates[0]=' ';
	while ((en = job_registry_get_next(rha, fd)) != NULL)
	{
	    if(((now - en->mdate) > finalstate_query_interval) && en->status!=3 && en->status!=4)
	    {
		/* create the constraint that will be used in condor_history command in FinalStateQuery*/
		sprintf(constraint," %s",en->batch_id);
		if (en->status==0) sprintf(constraint2," u");
		if (en->status==1) sprintf(constraint2," q");
		if (en->status==2) sprintf(constraint2," r");
		if (en->status==5) sprintf(constraint2," h");
		query=realloc(query,strlen(query)+strlen(constraint)+1);
		queryStates=realloc(queryStates,strlen(queryStates)+strlen(constraint2)+1);
		strcat(query,constraint);
		strcat(queryStates,constraint2);
		runfinal=TRUE;
	    }
	    /* Assign Status=4 and ExitStatus=-1 to all entries that after alldone_interval are still not in a final state(3 or 4) */
	    if((now - en->mdate > alldone_interval) && en->status!=3 && en->status!=4 && !runfinal)
	    {
		time_t now;
		now=time(0);
		sprintf(string_now,"%d",now);
		AssignState(en->batch_id,"4" ,"-1","\0","\0",string_now);
		free(string_now);
	    }
	   free(en);
	}
	if(runfinal){
	    if((query_err=calloc((int)strlen(query),1)) == 0)
		sysfatal("can't malloc query_err %r");
	    FinalStateQuery(query,queryStates,query_err);
	    free(query_err);
	}
	free(query);
	free(queryStates);
	fclose(fd);
	if (runfinal){
	    runfinal=FALSE;
	    sleep (5);
	}else sleep (60);
    } //for

    job_registry_destroy(rha);
    return(0);
}


int FinalStateQuery(char *query,char *queryStates, char *query_err){

    char line[STR_CHARS],fail[6],qExit[10],qFailed[10],qHostname[100],qStatus[2],command_string[100];
    char **saveptr1,**saveptr2,**list_query,**list_queryStates;
    FILE *file_output;
    int numQuery=0,numQueryStates=0,j=0,l=0,cont=0,cont2=0, nq=0;
    time_t now;
    char string_now[11];
    job_registry_entry en;
    
    numQuery=strtoken(query,' ',&list_query);
    nq=numQuery;
    numQueryStates=strtoken(queryStates,' ',&list_queryStates);
    if (numQuery!=numQueryStates) return 1;
    
    sprintf(command_string,"%s/qstat -u '*'",sge_binpath);
    if (debug) do_log(debuglogfile, debug, 1, "+-+line 433, command_string:%s\n",command_string);
    
    //load in qstatJob list of jobids from qstat command exec
    file_output = popen(command_string,"r");
    if (file_output == NULL) return 0;
    while (fgets(line,sizeof(line), file_output) != NULL){
	cont=strtoken(line, ' ', &saveptr1);
	if ((strcmp(saveptr1[0],"job-ID")!=0)&&(strncmp(saveptr1[0],"-",1)!=0)){
	    for (l=0;l<nq;l++){
		if (strcmp(list_query[l],saveptr1[0])==0){
		    if (strcmp(list_queryStates[l],saveptr1[4])!=0){
			now=time(0);
			sprintf(string_now,"%d",now);
			if (strcmp(saveptr1[4],"u")==0){
			    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
			    en.status=0;
			    en.exitcode=0;
			    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"0");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
			    en.udate=now;
			    if ((ret=job_registry_update(rha, &en)) < 0){
				fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
				perror("");
			    }
			}
			if (strcmp(saveptr1[4],"q")==0){
			    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
			    en.status=1;
			    en.exitcode=0;
			    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"0");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
			    en.udate=now;
			    if ((ret=job_registry_update(rha, &en)) < 0){
				fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
				perror("");
			    }
			}
			if (strcmp(saveptr1[4],"r")==0){
			    cont2=strtoken(saveptr1[7], '@', &saveptr2);
			    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
			    en.status=2;
			    en.exitcode=0;
			    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,saveptr2[1]);
			    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"0");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
			    en.udate=now;
			    if ((ret=job_registry_update(rha, &en)) < 0){
				fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
				perror("");
			    }
			    freetoken(&saveptr2,cont2);
			}
			if ((strcmp(saveptr1[4],"hr")==0)||strcmp(saveptr1[4],"hqw")==0){
			    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
			    en.status=5;
			    en.exitcode=0;
			    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"0");
			    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
			    en.udate=now;
			    if ((ret=job_registry_update(rha, &en)) < 0){
				fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
				perror("");
			    }
			}
		    }
		    //i must put out element from query
		    for (j=l;j<nq;j++)
			if (list_query[j+1]!=NULL) strcpy(list_query[j],list_query[j+1]);
		    for (j=l;j<nq;j++)
			if (list_queryStates[j+1]!=NULL) strcpy(list_queryStates[j],list_queryStates[j+1]);
		    nq--;
		    break;
		}
	    }
	}
	line[0]='\0';
	freetoken(&saveptr1,cont);
    }
    pclose( file_output );
    sprintf(query_err,"\0");
    //now we have check in list_query only states that not change status 
    //because they're not in qstat result
    for (l=0; l<nq; l++){
	sprintf(command_string,"%s/qacct -j %s",sge_binpath,list_query[l]);
	if (debug) do_log(debuglogfile, debug, 1, "+-+line 520,command_string:%s\n",command_string);
	file_output = popen(command_string,"r");
	if (file_output == NULL) return 1;
	//if a job number is here means that job was in query previously and
	//if now it's not in query and not finished (NULL qstat) it was deleted 
	//or it's on transition time
	if (fgets( line,sizeof(line), file_output )==NULL){
	    strcat(query_err,list_query[l]);
	    strcat(query_err," ");
	    pclose( file_output );
	    continue;
	}

	//there is no problem to lost first line with previous fgets, because 
	//it's only a line of =============================================
	while (fgets( line,sizeof(line), file_output )!=NULL){
	    cont=strtoken(line, ' ', &saveptr1);
	    if (strcmp(saveptr1[0],"hostname")==0) strcpy(qHostname,saveptr1[1]);;
	    if (strcmp(saveptr1[0],"failed")==0) strcpy(qFailed,saveptr1[1]);
	    if (strcmp(saveptr1[0],"exit_status")==0) strcpy(qExit,saveptr1[1]);
	    freetoken(&saveptr1,cont);
	}
	pclose( file_output );
	now=time(0);
	sprintf(string_now,"%d",now);
	if ((strcmp(qExit,"137")==0)||(strcmp(qExit,"143")==0)){
	    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
	    en.status=3;
	    en.exitcode=atoi(qExit);
	    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,qHostname);
	    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"");
	    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
	    en.udate=now;
	    if ((ret=job_registry_update(rha, &en)) < 0){
		fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
		perror("");
	    }else job_registry_unlink_proxy(rha, &en);
	}else{
	    JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,list_query[l]);
	    en.status=4;
	    en.exitcode=atoi(qExit);
	    JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,qHostname);
	    JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,qFailed);
	    JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
	    en.udate=now;
	    if ((ret=job_registry_update(rha, &en)) < 0){
		fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
		perror("");
	    }else job_registry_unlink_proxy(rha, &en);
	}
    }
    freetoken(&list_query,numQuery);
    freetoken(&list_queryStates,numQueryStates);
    if (debug) do_log(debuglogfile, debug, 1, "+-+query_err:%s\n",query_err);
    //now check acumulated error jobids to verify if they are an error or not
    if (strcmp(query_err,"\0")!=0){
	sleep(60);
	cont=0;
	int n=0;
	char cmd[10]="\0";
	
	cont=strtoken(query_err, ' ', &list_query);
	
	while (n < cont){
	    if(list_query[n]) strcpy(cmd,list_query[n]);
	    else return 1;
	    sprintf(command_string,"%s/qacct -j %s",sge_binpath,cmd);
	    if (debug) do_log(debuglogfile, debug, 1, "+-+line 587 error, command_string:%s\n",command_string);
	    file_output = popen(command_string,"r");
	    if (file_output == NULL) return 1;

	    //if a job number is here means that job was in query previously and
	    //if now it's not in query and not finished (NULL qstat) it was deleted 
	    if (fgets( line,sizeof(line), file_output )==NULL){
		JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,cmd);
		en.status=3;
		en.exitcode=3;
		JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"");
		JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"reason=3");
		now=time(0);
		sprintf(string_now,"%d",now);
		JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
		en.udate=now;
		if ((ret=job_registry_update(rha, &en)) < 0){
		    fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
		    perror("");
		}else job_registry_unlink_proxy(rha, &en);
		pclose( file_output );
		n++;
		continue;
	    }
	    //there is no problem to lost first line with previous fgets, because 
	    //it's only a line of =============================================
	    while (fgets( line,sizeof(line), file_output )!=NULL){
		cont=strtoken(line, ' ', &saveptr1);
		if (strcmp(saveptr1[0],"hostname")==0) strcpy(qHostname,saveptr1[1]);
		if (strcmp(saveptr1[0],"failed")==0) strcpy(qFailed,saveptr1[1]);
		if (strcmp(saveptr1[0],"exit_status")==0) strcpy(qExit,saveptr1[1]);
		freetoken(&saveptr1,cont);
	    }
	    pclose( file_output );
	    now=time(0);
	    sprintf(string_now,"%d",now);
	    if ((strcmp(qExit,"137")==0)||(strcmp(qExit,"143")==0)){
		JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,cmd);
		en.status=3;
		en.exitcode=atoi(qExit);
		JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,qHostname);
		JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"");
		JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
		en.udate=now;
		if ((ret=job_registry_update(rha, &en)) < 0){
		    fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
		    perror("");
		}else job_registry_unlink_proxy(rha, &en);
	    }else{
		JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,cmd);
		en.status=4;
		en.exitcode=atoi(qExit);
		JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,qHostname);
		JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,qFailed);
		JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
		en.udate=now;
		if ((ret=job_registry_update(rha, &en)) < 0){
		    fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
		    perror("");
		}else job_registry_unlink_proxy(rha, &en);
	    }
	    n++;
	}
	freetoken(&list_query,cont);
    }
    return 0;
}

int AssignState (char *element, char *status, char *exit, char *reason, char *wn, char *udate){
    char **id_element;
    job_registry_entry en;
    time_t now;
    char *string_now=NULL;
    int i=0;
    int n=strtoken(element, '.', &id_element);
    
    if(id_element[0]){
	JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,id_element[0]);
	en.status=atoi(status);
	en.exitcode=atoi(exit);
	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,wn);
	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,reason);
	now=time(0);
	string_now=make_message("%d",now);
	JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now)
	en.udate=now;
	free(string_now);
    }else{
	if((element=calloc(STR_CHARS,1)) == 0){
	    sysfatal("can't malloc cmd in GetAndSend: %r");
	}
    }
    if ((ret=job_registry_update(rha, &en)) < 0){
	fprintf(stderr,"Update of record returns %d: \nJobId: %d", ret,en.batch_id);
	perror("");
    }else{
	if (en.status == REMOVED || en.status == COMPLETED){
	    job_registry_unlink_proxy(rha, &en);
	}
    }
    freetoken(&id_element,n);
    return 0;
}

void sighup()
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
    printf("Usage: BUpdaterSGE [OPTION...]\n");
    printf("  -o, --nodaemon     do not run as daemon\n");
    printf("  -v, --version      print version and exit\n");
    printf("\n");
    printf("Help options:\n");
    printf("  -?, --help         Show this help message\n");
    printf("  --usage            Display brief usage message\n");
    printf("  --test            Display a error message\n");
    exit(EXIT_SUCCESS);
}

int 
short_usage()
{
    printf("Usage: BUpdaterSGE [-ov?] [-o|--nodaemon] [-v|--version] [-?|--help] [--usage]\n");
    exit(EXIT_SUCCESS);
}
