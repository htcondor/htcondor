/*
#  File:     BUpdaterPBS.c
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

#include "BUpdaterPBS.h"

int main(int argc, char *argv[]){

	FILE *fd;
	job_registry_entry *en;
	time_t now;
	time_t purge_time=0;
	time_t last_consistency_check=0;
	char *pidfile=NULL;
	char *final_string=NULL;
	char *cp=NULL;
	char *tpath;
	char *tspooldir;
	
	char *first_duplicate=NULL;
	
	struct pollfd *remupd_pollset = NULL;
	int remupd_nfds;
	
	int version=0;
	int first=TRUE;
	int tmptim;
	int finstr_len=0;
	int loop_interval=DEFAULT_LOOP_INTERVAL;
	
	int fsq_ret=0;
	
	int c;				
	
	pthread_t RecUpdNetThd;

	int confirm_time=0;	

        static int help;
        static int short_help;
	
	bact.njobs = 0;
	bact.jobs = NULL;
	
	while (1) {
		static struct option long_options[] =
		{
		{"help",      no_argument,     &help,       1},
		{"usage",     no_argument,     &short_help, 1},
		{"nodaemon",  no_argument,       0, 'o'},
		{"version",   no_argument,       0, 'v'},
		{"prefix",    required_argument, 0, 'p'},
		{0, 0, 0, 0}
		};

		int option_index = 0;
     
		c = getopt_long (argc, argv, "vop:",long_options, &option_index);
     
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
	
        ret = config_get("pbs_binpath",cha);
        if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key pbs_binpath not found\n",argv0);
        } else {
                pbs_binpath=strdup(ret->value);
                if(pbs_binpath == NULL){
                        sysfatal("strdup failed for pbs_binpath in main: %r");
                }
        }
	
        ret = config_get("pbs_spoolpath",cha);
        if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key pbs_spoolpath not found\n",argv0);
        } else {
                pbs_spoolpath=strdup(ret->value);
                if(pbs_spoolpath == NULL){
                        sysfatal("strdup failed for pbs_spoolpath in main: %r");
                }

		tpath=make_message("%s/server_logs",pbs_spoolpath);
		if (opendir(tpath)==NULL){
			do_log(debuglogfile, debug, 1, "%s: dir %s does not exist or is not readable. Trying now pbs commands\n",argv0,tpath);
                	tspooldir=GetPBSSpoolPath(pbs_binpath);
                	free(tpath);
                	tpath=make_message("%s/server_logs",tspooldir);
                	free(tspooldir);
                	if (opendir(tpath)==NULL){
				do_log(debuglogfile, debug, 1, "dir %s does not exist or is not readable (using pbs commands)",tpath);
				sysfatal("dir %s does not exist or is not readable (using pbs commands): %r",tpath);
                	}
		}
		free(tpath);
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

	ret = config_get("tracejob_logs_to_read",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key tracejob_logs_to_read not found using the default:%d\n",argv0,tracejob_logs_to_read);
	} else {
		tracejob_logs_to_read=atoi(ret->value);
	}
	
	ret = config_get("bupdater_loop_interval",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key bupdater_loop_interval not found using the default:%d\n",argv0,loop_interval);
	} else {
		loop_interval=atoi(ret->value);
	}
	
	ret = config_get("bupdater_consistency_check_interval",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key bupdater_consistency_check_interval not found using the default:%d\n",argv0,bupdater_consistency_check_interval);
	} else {
		bupdater_consistency_check_interval=atoi(ret->value);
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
	
	ret = config_get("pbs_batch_caching_enabled",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key pbs_batch_caching_enabled not found using default\n",argv0,pbs_batch_caching_enabled);
	} else {
		pbs_batch_caching_enabled=strdup(ret->value);
                if(pbs_batch_caching_enabled == NULL){
                        sysfatal("strdup failed for pbs_batch_caching_enabled in main: %r");
                }
	}
	
	ret = config_get("batch_command_caching_filter",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key batch_command_caching_filter not found using default\n",argv0,batch_command_caching_filter);
	} else {
		batch_command_caching_filter=strdup(ret->value);
                if(batch_command_caching_filter == NULL){
                        sysfatal("strdup failed for batch_command_caching_filter in main: %r");
                }
	}
	
	batch_command=(strcmp(pbs_batch_caching_enabled,"yes")==0?make_message("%s ",batch_command_caching_filter):make_message(""));

	ret = config_get("job_registry_use_mmap",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap not found. Default is NO\n",argv0);
	} else {
		do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap is set to %s\n",argv0,ret->value);
	}
	
	ret = config_get("tracejob_max_output",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key tracejob_max_output not found using default\n",argv0,tracejob_max_output);
	} else {
		tracejob_max_output==atoi(ret->value);
	}
	
	remupd_conf = config_get("job_registry_add_remote",cha);
	if (remupd_conf == NULL){
		do_log(debuglogfile, debug, 1, "%s: key job_registry_add_remote not found\n",argv0);
	}else{
		if (job_registry_updater_setup_receiver(remupd_conf->values,remupd_conf->n_values,&remupd_head) < 0){
			do_log(debuglogfile, debug, 1, "%s: Cannot set network receiver(s) up for remote update\n",argv0);
			fprintf(stderr,"%s: Cannot set network receiver(s) up for remote update \n",argv0);
       		}
 
		if (remupd_head == NULL){
			do_log(debuglogfile, debug, 1, "%s: Cannot find values for network endpoints in configuration file (attribute 'job_registry_add_remote').\n",argv0);
			fprintf(stderr,"%s: Cannot find values for network endpoints in configuration file (attribute 'job_registry_add_remote').\n", argv0);
		}

		if ((remupd_nfds = job_registry_updater_get_pollfd(remupd_head, &remupd_pollset)) < 0){
			do_log(debuglogfile, debug, 1, "%s: Cannot setup poll set for receiving data.\n",argv0);
    			fprintf(stderr,"%s: Cannot setup poll set for receiving data.\n", argv0);
		}
		if (remupd_pollset == NULL || remupd_nfds == 0){
			do_log(debuglogfile, debug, 1, "%s: No poll set available for receiving data.\n",argv0);
			fprintf(stderr,"%s: No poll set available for receiving data.\n",argv0);
		}
	
	}
	
	if( !nodmn ) daemonize();


	if( pidfile ){
		writepid(pidfile);
		free(pidfile);
	}
	
	rha=job_registry_init(registry_file, BY_BATCH_ID);
	if (rha == NULL){
		do_log(debuglogfile, debug, 1, "%s: Error initialising job registry %s\n",argv0,registry_file);
		fprintf(stderr,"%s: Error initialising job registry %s :",argv0,registry_file);
		perror("");
	}
	
	if (remupd_conf != NULL){
		pthread_create(&RecUpdNetThd, NULL, (void *(*)(void *))ReceiveUpdateFromNetwork, (void *)NULL);
	
		if (job_registry_updater_setup_sender(remupd_conf->values,remupd_conf->n_values,0,&remupd_head_send) < 0){
			do_log(debuglogfile, debug, 1, "%s: Cannot set network sender(s) up for remote update\n",argv0);
			fprintf(stderr,"%s: Cannot set network sender(s) up for remote update \n",argv0);
       		}
		if (remupd_head_send == NULL){
			do_log(debuglogfile, debug, 1, "%s: Cannot find values for network endpoints in configuration file (attribute 'job_registry_add_remote').\n",argv0);
			fprintf(stderr,"%s: Cannot find values for network endpoints in configuration file (attribute 'job_registry_add_remote').\n", argv0);
		}
	}

	config_free(cha);

	for(;;){
		/* Purge old entries from registry */
		now=time(0);
		if(now - purge_time > 86400){
			if(job_registry_purge(registry_file, now-purge_interval,0)<0){
				do_log(debuglogfile, debug, 1, "%s: Error purging job registry %s\n",argv0,registry_file);
                	        fprintf(stderr,"%s: Error purging job registry %s :",argv0,registry_file);
                	        perror("");

			}else{
				purge_time=time(0);
			}
		}
		
		now=time(0);
		if(now - last_consistency_check > bupdater_consistency_check_interval){
			if(job_registry_check_index_key_uniqueness(rha,&first_duplicate)==JOB_REGISTRY_FAIL){
				do_log(debuglogfile, debug, 1, "%s: Found job registry duplicate entry. The first one is:%s\n",argv0,first_duplicate);
               	        	fprintf(stderr,"%s: Found job registry duplicate entry. The first one is:%s",argv0,first_duplicate);
 
			}else{
				last_consistency_check=time(0);
			}
		}
	       
		IntStateQuery();
		
		fd = job_registry_open(rha, "r");
		
		if (fd == NULL){
			do_log(debuglogfile, debug, 1, "%s: Error opening job registry %s\n",argv0,registry_file);
			fprintf(stderr,"%s: Error opening job registry %s :",argv0,registry_file);
			perror("");
			sleep(loop_interval);
			continue;
		}
		if (job_registry_rdlock(rha, fd) < 0){
			do_log(debuglogfile, debug, 1, "%s: Error read locking job registry %s\n",argv0,registry_file);
			fprintf(stderr,"%s: Error read locking job registry %s :",argv0,registry_file);
			perror("");
			sleep(loop_interval);
			continue;
		}
		job_registry_firstrec(rha,fd);
		fseek(fd,0L,SEEK_SET);
		
		first=TRUE;
		
		while ((en = job_registry_get_next(rha, fd)) != NULL){
			if((bupdater_lookup_active_jobs(&bact, en->batch_id) != BUPDATER_ACTIVE_JOBS_SUCCESS) && en->status!=REMOVED && en->status!=COMPLETED){
				
				confirm_time=atoi(en->updater_info);
				if(confirm_time==0){
					confirm_time=en->mdate;
				}
			
				/* Assign Status=4 and ExitStatus=999 to all entries that after alldone_interval are still not in a final state(3 or 4)*/
				if(now-confirm_time>alldone_interval){
					AssignFinalState(en->batch_id);	
					free(en);
					continue;
				}
			
				if((now-confirm_time>finalstate_query_interval) && (now > next_finalstatequery)){
					if((final_string=realloc(final_string,finstr_len + strlen(en->batch_id) + 2)) == 0){
                       	        		sysfatal("can't malloc final_string: %r");
					} else {
						if (finstr_len == 0) final_string[0] = '\000';
					}
 					strcat(final_string,en->batch_id);
					strcat(final_string,":");
					finstr_len=strlen(final_string);
					runfinal=TRUE;
				}
				
			}
			free(en);
		}
		
		if(runfinal){
			if (final_string[finstr_len-1] == ':' && (cp = strrchr (final_string, ':')) != NULL){
				*cp = '\0';
			}
				
			if(fsq_ret != 0){
				fsq_ret=FinalStateQuery(final_string,tracejob_logs_to_read);
			}else{
				fsq_ret=FinalStateQuery(final_string,1);
			}
			
			runfinal=FALSE;
		}
		if (final_string != NULL){
			free(final_string);		
			final_string = NULL;
			finstr_len = 0;
		}
		fclose(fd);		
		sleep(loop_interval);
	}
	
	job_registry_destroy(rha);
		
	return 0;
	
}

int 
ReceiveUpdateFromNetwork()
{
	char *proxy_path, *proxy_subject;
	int timeout_ms = 0;
	int ent, ret, prret, rhret;
	job_registry_entry *nen;
	job_registry_entry *ren;
  
	proxy_path = NULL;
	proxy_subject = NULL;
	
	while (nen = job_registry_receive_update(remupd_pollset, remupd_nfds,timeout_ms, &proxy_subject, &proxy_path)){
	
		JOB_REGISTRY_ASSIGN_ENTRY(nen->subject_hash,"\0");
		JOB_REGISTRY_ASSIGN_ENTRY(nen->proxy_link,"\0");
		
		if ((ren=job_registry_get(rha, nen->batch_id)) == NULL){
			if ((ret=job_registry_append(rha, nen)) < 0){
				fprintf(stderr,"%s: Warning: job_registry_append returns %d: ",argv0,ret);
				perror("");
			} 
		}else{
		
			if(ren->subject_hash!=NULL && strlen(ren->subject_hash) && ren->proxy_link!=NULL && strlen(ren->proxy_link)){
				JOB_REGISTRY_ASSIGN_ENTRY(nen->subject_hash,ren->subject_hash);
				JOB_REGISTRY_ASSIGN_ENTRY(nen->proxy_link,ren->proxy_link);
			}else{
				if (proxy_path != NULL && strlen(proxy_path) > 0){
					prret = job_registry_set_proxy(rha, nen, proxy_path);
     			 		if (prret < 0){
						do_log(debuglogfile, debug, 1, "%s: warning: setting proxy to %s\n",argv0,proxy_path);
        					fprintf(stderr,"%s: warning: setting proxy to %s: ",argv0,proxy_path);
        					perror("");
        					/* Make sure we don't renew non-existing proxies */
						nen->renew_proxy = 0;  		
					}
					free(proxy_path);
  
					nen->subject_hash[0] = '\000';
					if (proxy_subject != NULL && strlen(proxy_subject) > 0){
						job_registry_compute_subject_hash(nen, proxy_subject);
						rhret = job_registry_record_subject_hash(rha, nen->subject_hash, proxy_subject, TRUE);  
						if (rhret < 0){
							do_log(debuglogfile, debug, 1, "%s: warning: recording proxy subject %s (hash %s)\n",argv0, proxy_subject, nen->subject_hash);
							fprintf(stderr,"%s: warning: recording proxy subject %s (hash %s): ",argv0, proxy_subject, nen->subject_hash);
							perror("");
						}
					}
					free(proxy_subject);
  
				}
			}
			if(job_registry_need_update(ren,nen,JOB_REGISTRY_UPDATE_ALL)){
				if ((ret=job_registry_update(rha, nen)) < 0){
					fprintf(stderr,"%s: Warning: job_registry_update returns %d: ",argv0,ret);
					perror("");
				}
			} 
		}
		free(nen);
	}
  
	return 0;
}

int
IntStateQuery()
{
/*
qstat -f

Job Id: 11.cream-12.pd.infn.it
    Job_Name = cream_579184706
    job_state = R
    ctime = Wed Apr 23 11:39:55 2008
    exec_host = cream-wn-029.pn.pd.infn.it/0
*/

/*
 Filled entries:
 batch_id
 wn_addr
 status
 udate
 
 Filled by submit script:
 blah_id 
 
 Unfilled entries:
 exitreason
*/


        FILE *fp;
	char *line=NULL;
	char **token;
	int maxtok_t=0;
	job_registry_entry en;
	int ret;
	char *timestamp;
	time_t tmstampepoch;
	char *batch_str=NULL;
	char *wn_str=NULL; 
        char *twn_str=NULL;
        char *status_str=NULL;
	char *ex_str=NULL;
	int  ex_code=0; 
	char *cp=NULL;
	char *command_string=NULL;
	job_registry_entry *ren=NULL;
	int first=TRUE;
	time_t now;
	char *string_now=NULL;

	command_string=make_message("%s%s/qstat -f",batch_command,pbs_binpath);
	fp = popen(command_string,"r");

	en.status=UNDEFINED;
	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
	JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,"\0");
	en.exitcode=-1;
	bupdater_free_active_jobs(&bact);

	if(fp!=NULL){
		while(!feof(fp) && (line=get_line(fp))){
			if(line && strlen(line)==0){
				free(line);
				continue;
			}
			if ((cp = strrchr (line, '\n')) != NULL){
				*cp = '\0';
			}
			do_log(debuglogfile, debug, 3, "%s: line in IntStateQuery is:%s\n",argv0,line);
			now=time(0);
			string_now=make_message("%d",now);
			if(line && strstr(line,"Job Id: ")){
				if(!first && en.status!=UNDEFINED && ren && ren->status!=REMOVED && ren->status!=COMPLETED){
                        		if ((ret=job_registry_update_recn_select(rha, &en, ren->recnum,
					JOB_REGISTRY_UPDATE_WN_ADDR|
					JOB_REGISTRY_UPDATE_STATUS|
					JOB_REGISTRY_UPDATE_UDATE|
					JOB_REGISTRY_UPDATE_UPDATER_INFO|
					JOB_REGISTRY_UPDATE_EXITCODE|
					JOB_REGISTRY_UPDATE_EXITREASON)) < 0){
						if(ret != JOB_REGISTRY_NOT_FOUND){
                	                		fprintf(stderr,"Update of record returns %d: ",ret);
							perror("");
						}
					} else {
						if(ret==JOB_REGISTRY_SUCCESS){
							if (en.status == REMOVED || en.status == COMPLETED) {
								do_log(debuglogfile, debug, 2, "%s: registry update in IntStateQuery for: jobid=%s wn=%s status=%d exitcode=%d\n",argv0,en.batch_id,en.wn_addr,en.status,en.exitcode);
								job_registry_unlink_proxy(rha, &en);
							}else{
								do_log(debuglogfile, debug, 2, "%s: registry update in IntStateQuery for: jobid=%s wn=%s status=%d\n",argv0,en.batch_id,en.wn_addr,en.status);
							}
						}
						if (remupd_conf != NULL){
							if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
								do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in IntStateQuery\n",argv0);
							}
						}
					}
					en.status = UNDEFINED;
					JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
					JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
					JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,"\0");
					en.exitcode=-1;
				}				
                        	maxtok_t = strtoken(line, ':', &token);
				batch_str=strdel(token[1]," ");
				JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,batch_str);
				JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now);
				en.exitcode=-1;
				bupdater_push_active_job(&bact, en.batch_id);
				free(batch_str);
				freetoken(&token,maxtok_t);
				if(!first) free(ren);
				if ((ren=job_registry_get(rha, en.batch_id)) == NULL){
						fprintf(stderr,"Get of record returns error for %s ",en.batch_id);
						perror("");
				}
				first=FALSE;				
			}else if(line && strstr(line,"job_state = ")){	
				maxtok_t = strtoken(line, '=', &token);
				status_str=strdel(token[1]," ");
				if(status_str && strcmp(status_str,"Q")==0){ 
					en.status=IDLE;
					en.exitcode=-1;
					JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
				}else if(status_str && strcmp(status_str,"W")==0){ 
					en.status=IDLE;
					en.exitcode=-1;
				}else if(status_str && strcmp(status_str,"R")==0){ 
					en.status=RUNNING;
					en.exitcode=-1;
				}else if(status_str && strcmp(status_str,"C")==0){ 
					en.status=COMPLETED;
					JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
				}else if(status_str && strcmp(status_str,"H")==0){ 
					en.status=HELD;
					en.exitcode=-1;
					JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
				}
				free(status_str);
				freetoken(&token,maxtok_t);
			}else if(line && strstr(line,"unable to run job")){
				en.status=IDLE;	
				en.exitcode=-1;
			}else if(line && strstr(line,"exit_status = ")){
				maxtok_t = strtoken(line, '=', &token);
				ex_str=strdel(token[1]," ");
				ex_code=atoi(ex_str);
				if(ex_code==0){
					en.exitcode=0;
				}else if(ex_code==271){
					en.status=REMOVED;
                        		en.exitcode=-999;
				}else{
					en.exitcode=ex_code;
				}
				free(ex_str);
				freetoken(&token,maxtok_t);
			}else if(line && strstr(line,"exec_host = ")){	
				maxtok_t = strtoken(line, '=', &token);
				twn_str=strdup(token[1]);
                		if(twn_str == NULL){
                        		sysfatal("strdup failed for twn_str in IntStateQuery: %r");
                		}
				freetoken(&token,maxtok_t);
				maxtok_t = strtoken(twn_str, '/', &token);
				wn_str=strdel(token[0]," ");
				JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,wn_str);
				free(twn_str);
 				free(wn_str);
				freetoken(&token,maxtok_t);
			}else if(line && strstr(line,"mtime = ")){	
                        	maxtok_t = strtoken(line, ' ', &token);
				timestamp=make_message("%s %s %s %s %s",token[2],token[3],token[4],token[5],token[6]);
                        	tmstampepoch=str2epoch(timestamp,"L");
				free(timestamp);
				en.udate=tmstampepoch;
				freetoken(&token,maxtok_t);
			}
			free(line);
			free(string_now);

		}
		pclose(fp);
	}
	
	if(en.status!=UNDEFINED && ren && ren->status!=REMOVED && ren->status!=COMPLETED){
		if ((ret=job_registry_update_recn_select(rha, &en, ren->recnum,
		JOB_REGISTRY_UPDATE_WN_ADDR|
		JOB_REGISTRY_UPDATE_STATUS|
		JOB_REGISTRY_UPDATE_UDATE|
		JOB_REGISTRY_UPDATE_UPDATER_INFO|
		JOB_REGISTRY_UPDATE_EXITCODE|
		JOB_REGISTRY_UPDATE_EXITREASON)) < 0){
			if(ret != JOB_REGISTRY_NOT_FOUND){
				fprintf(stderr,"Update of record returns %d: ",ret);
				perror("");
			}
		} else {
			if(ret==JOB_REGISTRY_SUCCESS){
				if (en.status == REMOVED || en.status == COMPLETED) {
					do_log(debuglogfile, debug, 2, "%s: registry update in IntStateQuery for: jobid=%s wn=%s status=%d exitcode=%d\n",argv0,en.batch_id,en.wn_addr,en.status,en.exitcode);
					job_registry_unlink_proxy(rha, &en);
				}else{
					do_log(debuglogfile, debug, 2, "%s: registry update in IntStateQuery for: jobid=%s wn=%s status=%d\n",argv0,en.batch_id,en.wn_addr,en.status);
				}
			}
			if (remupd_conf != NULL){
				if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
					do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in IntStateQuery\n",argv0);
				}
			}
		}
	}				

	free(ren);
	free(command_string);
	return 0;
}

int
FinalStateQuery(char *input_string, int logs_to_read)
{
/*
tracejob -m -l -a <jobid>
In line:

04/23/2008 11:50:43  S    Exit_status=0 resources_used.cput=00:00:01 resources_used.mem=11372kb resources_used.vmem=52804kb
                          resources_used.walltime=00:10:15

there are:
udate for the final state (04/23/2008 11:50:43):
exitcode Exit_status=

*/

/*
 Filled entries:
 batch_id (a list of jobid is given, one for each tracejob call)
 status (always a final state 3 or 4)
 exitcode
 udate
 
 Filled by submit script:
 blah_id 
 
 Unfilled entries:
 exitreason
*/
/*
[root@cream-12 server_logs]# tracejob -m -l -a 13

Job: 13.cream-12.pd.infn.it

04/23/2008 11:40:27  S    enqueuing into cream_1, state 1 hop 1
04/23/2008 11:40:27  S    Job Queued at request of infngrid002@cream-12.pd.infn.it, owner = infngrid002@cream-12.pd.infn.it, job name =
                          cream_365713239, queue = cream_1
04/23/2008 11:40:28  S    Job Modified at request of root@cream-12.pd.infn.it
04/23/2008 11:40:28  S    Job Run at request of root@cream-12.pd.infn.it
04/23/2008 11:50:43  S    Exit_status=0 resources_used.cput=00:00:01 resources_used.mem=11372kb resources_used.vmem=52804kb
                          resources_used.walltime=00:10:15
04/23/2008 11:50:44  S    dequeuing from cream_1, state COMPLETE
*/

        FILE *fp;
	char *line=NULL;
	char **token;
	char **jobid;
	int maxtok_t=0,maxtok_j=0,k;
	job_registry_entry en;
	int ret;
	char *timestamp;
	time_t tmstampepoch;
	char *exit_str=NULL;
	int failed_count=0;
	int time_to_add=0;
	time_t now;
	char *cp=NULL;
	char *command_string=NULL;
	char *pbs_spool=NULL;
	char *string_now=NULL;
	int tracejob_line_counter=0;

	do_log(debuglogfile, debug, 3, "%s: input_string in FinalStateQuery is:%s\n",argv0,input_string);
	
	maxtok_j = strtoken(input_string, ':', &jobid);
	
	for(k=0;k<maxtok_j;k++){
	
		if(jobid[k] && strlen(jobid[k])==0) continue;

		pbs_spool=(pbs_spoolpath?make_message("-p %s ",pbs_spoolpath):make_message(""));
		command_string=make_message("%s%s/tracejob %s-m -l -a -n %d %s",batch_command,pbs_binpath,pbs_spool,logs_to_read,jobid[k]);
		free(pbs_spool);
		
		fp = popen(command_string,"r");
		
		do_log(debuglogfile, debug, 3, "%s: command_string in FinalStateQuery is:%s\n",argv0,command_string);

		/* en.status is set =0 (UNDEFINED) here and it is tested if it is !=0 before the registry update: the update is done only if en.status is !=0*/
		en.status=UNDEFINED;
		
		JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,jobid[k]);

		tracejob_line_counter=0;
		
		if(fp!=NULL){
			while(!feof(fp) && (line=get_line(fp))){
				if(line && strlen(line)==0){
					free(line);
					continue;
				}
				if(tracejob_line_counter>tracejob_max_output){
					do_log(debuglogfile, debug, 2, "%s: Tracejob output limit of %d lines reached. Skipping command.\n",argv0,tracejob_max_output);
					free(line);
					break;
				}
				if ((cp = strrchr (line, '\n')) != NULL){
					*cp = '\0';
					tracejob_line_counter++;
					
				}
                        	do_log(debuglogfile, debug, 3, "%s: line in FinalStateQuery is:%s\n",argv0,line);
				now=time(0);
				string_now=make_message("%d",now);
				if(line && (strstr(line,"Job deleted") || (strstr(line,"dequeuing from") && strstr(line,"state RUNNING")))){	
					maxtok_t = strtoken(line, ' ', &token);
					timestamp=make_message("%s %s",token[0],token[1]);
					tmstampepoch=str2epoch(timestamp,"A");
					free(timestamp);
					freetoken(&token,maxtok_t);
					en.udate=tmstampepoch;
					en.status=REMOVED;
                        		en.exitcode=-999;
					JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now);
					JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
				}else if(line && strstr(line," Exit_status=") && en.status != REMOVED){	
					maxtok_t = strtoken(line, ' ', &token);
					timestamp=make_message("%s %s",token[0],token[1]);
					tmstampepoch=str2epoch(timestamp,"A");
					exit_str=strdup(token[3]);
                			if(exit_str == NULL){
                        			sysfatal("strdup failed for exit_str in FinalStateQuery: %r");
                			}
					free(timestamp);
					freetoken(&token,maxtok_t);
					if(strstr(exit_str,"Exit_status=")){
						maxtok_t = strtoken(exit_str, '=', &token);
						if(maxtok_t == 2){
                        				en.exitcode=atoi(token[1]);
							freetoken(&token,maxtok_t);
						}else{
							en.exitcode=-1;
						}
					}else{
						en.exitcode=-1;
					}
					free(exit_str);
					en.udate=tmstampepoch;
					en.status=COMPLETED;
					JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now);
					JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
				}
				free(string_now);
				free(line);
			}
			pclose(fp);
		}
		
		if(en.status !=UNDEFINED && en.status!=IDLE){
			if ((ret=job_registry_update_select(rha, &en,
			JOB_REGISTRY_UPDATE_UDATE |
			JOB_REGISTRY_UPDATE_STATUS |
			JOB_REGISTRY_UPDATE_UPDATER_INFO |
			JOB_REGISTRY_UPDATE_EXITCODE |
			JOB_REGISTRY_UPDATE_EXITREASON )) < 0){
				if(ret != JOB_REGISTRY_NOT_FOUND){
					fprintf(stderr,"Update of record returns %d: ",ret);
					perror("");
				}
			} else {
				do_log(debuglogfile, debug, 2, "%s: registry update in FinalStateQuery for: jobid=%s exitcode=%d status=%d\n",argv0,en.batch_id,en.exitcode,en.status);
				if (en.status == REMOVED || en.status == COMPLETED){
					job_registry_unlink_proxy(rha, &en);
				}
				if (remupd_conf != NULL){
					if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
						do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in FinalStateQuery\n",argv0);
					}
				}
			}
		}else{
			failed_count++;
		}		
		free(command_string);
	}
	
	now=time(0);
	if(failed_count>10){
		failed_count=10;
	}
	time_to_add=pow(failed_count,1.5);
	next_finalstatequery=now+time_to_add;
	do_log(debuglogfile, debug, 3, "%s: next FinalStatequery will be in %d seconds\n",argv0,time_to_add);
	
	freetoken(&jobid,maxtok_j);
	return failed_count;
}

int AssignFinalState(char *batchid){

	job_registry_entry en;
	int ret,i;
	time_t now;

	now=time(0);
	
	JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,batchid);
	en.status=COMPLETED;
	en.exitcode=999;
	en.udate=now;
	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
		
	if ((ret=job_registry_update(rha, &en)) < 0){
		if(ret != JOB_REGISTRY_NOT_FOUND){
			fprintf(stderr,"Update of record %d returns %d: ",i,ret);
			perror("");
		}
	} else {
		do_log(debuglogfile, debug, 2, "%s: registry update in AssignStateQuery for: jobid=%s creamjobid=%s status=%d\n",argv0,en.batch_id,en.user_prefix,en.status);
		job_registry_unlink_proxy(rha, &en);
		if (remupd_conf != NULL){
			if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
				do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in AssignFinalState\n",argv0);
			}
		}
	}

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
	printf("Usage: BUpdaterPBS [OPTION...]\n");
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
	printf("Usage: BUpdaterPBS [-ov?] [-o|--nodaemon] [-v|--version] [-?|--help] [--usage]\n");
	exit(EXIT_SUCCESS);
}
