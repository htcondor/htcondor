/*
#  File:     BUpdaterCondor.c
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

#include "BUpdaterCondor.h"

int main(int argc, char *argv[]){

	FILE *fd;
	job_registry_entry *en;
	time_t now;
	time_t purge_time=0;
	time_t last_consistency_check=0;
	char *constraint=NULL;
	char *tconstraint=NULL;
	char *query=NULL;
	char *q=NULL;
	char *pidfile=NULL;
	char *first_duplicate=NULL;
	
	struct pollfd *remupd_pollset = NULL;
	int remupd_nfds;
	
	int version=0;
	int qlen=0;
	int first=TRUE;
        int tmptim=0;
	int loop_interval=DEFAULT_LOOP_INTERVAL;
	
	int c;				
	int condor_ver=0;
	int max_constr_len=0;
	char *toadd=NULL;
	
	pthread_t RecUpdNetThd;

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
	
        ret = config_get("condor_binpath",cha);
        if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key condor_binpath not found\n",argv0);
        } else {
                condor_binpath=strdup(ret->value);
                if(condor_binpath == NULL){
                        sysfatal("strdup failed for condor_binpath in main: %r");
                }
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
	
	ret = config_get("condor_batch_caching_enabled",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key condor_batch_caching_enabled not found using default\n",argv0,condor_batch_caching_enabled);
	} else {
		condor_batch_caching_enabled=strdup(ret->value);
                if(condor_batch_caching_enabled == NULL){
                        sysfatal("strdup failed for condor_batch_caching_enabled in main: %r");
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
	
	batch_command=(strcmp(condor_batch_caching_enabled,"yes")==0?make_message("%s ",batch_command_caching_filter):make_message(""));

	ret = config_get("job_registry_use_mmap",cha);
	if (ret == NULL){
		do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap not found. Default is NO\n",argv0);
	} else {
		do_log(debuglogfile, debug, 1, "%s: key job_registry_use_mmap is set to %s\n",argv0,ret->value);
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
	
	/* Get condor version to use or not a workaround for a bug in the max length of the constraint (-constraint) string
	   that can be passed to condor_history. The limit is 511 byte if the version is prior 5.6.2 and 5.7.0*/
	   
	condor_ver=GetCondorVersion();
	if(condor_ver>=762){
		max_constr_len=511;
	}else{
		max_constr_len=-1;
	}	

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

			if(en->status!=REMOVED && en->status!=COMPLETED){
			
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
				
				if(now-confirm_time>finalstate_query_interval){
					/* create the constraint that will be used in condor_history command in FinalStateQuery*/
					if(first){
						toadd=make_message("");
					}else{
						toadd=make_message(" || ");
					}	
					if(first) first=FALSE;
					
					tconstraint=make_message("ClusterId==%s",en->batch_id);
					
					if (query != NULL){
						qlen = strlen(query);
					}else{
						qlen = 0;
					}
					if(max_constr_len > 0 && tconstraint && ((strlen(tconstraint)+qlen)>max_constr_len)){
						constraint=make_message(";%s",tconstraint);
					}else{
						constraint=make_message("%s%s",toadd,tconstraint);
					}
					free(tconstraint);
					
					q=realloc(query,qlen+strlen(constraint)+4);
					
					if(q != NULL){
						if (query != NULL){
							strcat(q,constraint);
						}else{
							strcpy(q,constraint);
						}
						query=q;	
					}else{
						sysfatal("can't realloc query: %r");
					}
					free(constraint);
					runfinal=TRUE;
				}
			}
			free(en);
		}
		
		if(runfinal){
			FinalStateQuery(query);
			runfinal=FALSE;
		}
		if (query != NULL){
			free(query);
			query = NULL;
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
 Output format for status query for unfinished jobs for condor:
 batch_id   user      status     executable     exitcode   udate(timestamp_for_current_status)
 22018     gliteuser  2          /bin/sleep     0          1202288920

 Filled entries:
 batch_id
 status
 exitcode
 udate
 
 Filled by suhmit script:
 blah_id 
 
 Unfilled entries:
 wn_addr
 exitreason
*/

        FILE *fp;
	char *line=NULL;
	char **token;
	int maxtok_t=0;
	job_registry_entry en;
	int ret=0;
	char *cp=NULL; 
	char *command_string=NULL;
	job_registry_entry *ren=NULL;
	time_t now;
	char *string_now=NULL;

	command_string=make_message("%s%s/condor_q -format \"%%d \" ClusterId -format \"%%s \" Owner -format \"%%d \" JobStatus -format \"%%s \" Cmd -format \"%%s \" ExitStatus -format \"%%s\\n\" EnteredCurrentStatus|grep -v condorc-",batch_command,condor_binpath);
	do_log(debuglogfile, debug, 2, "%s: command_string in IntStateQuery:%s\n",argv0,command_string);
	fp = popen(command_string,"r");

	if(fp!=NULL){
		while(!feof(fp) && (line=get_line(fp))){
			do_log(debuglogfile, debug, 3, "%s: Line in ISQ:%s\n",argv0,line);
			if(line && (strlen(line)==0 || strncmp(line,"JOBID",5)==0)){
				free(line);
				continue;
			}
			if ((cp = strrchr (line, '\n')) != NULL){
				*cp = '\0';
			}
	
			maxtok_t = strtoken(line, ' ', &token);
			if (maxtok_t < 6){
				freetoken(&token,maxtok_t);
				free(line);
				continue;
			}
			now=time(0);
			string_now=make_message("%d",now);
		
			JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,token[0]);
			JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now);
			en.status=atoi(token[2]);
			en.exitcode=atoi(token[4]);
			en.udate=atoi(token[5]);
			JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
			JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
			
			if ((ren=job_registry_get(rha, en.batch_id)) == NULL){
					fprintf(stderr,"Get of record returns error for %s ",en.batch_id);
					perror("");
			}
				
			if(en.status!=UNDEFINED && ren && ren->status!=REMOVED && ren->status!=COMPLETED){

				if ((ret=job_registry_update_recn(rha, &en, ren->recnum)) < 0){
					if(ret != JOB_REGISTRY_NOT_FOUND){
						fprintf(stderr,"Update of record returns %d: ",ret);
						perror("");
					}
				} else {
					if(ret==JOB_REGISTRY_SUCCESS){
						if (en.status == REMOVED || en.status == COMPLETED){
							job_registry_unlink_proxy(rha, &en);
						}else{
							do_log(debuglogfile, debug, 2, "%s: registry update in IntStateQuery for: jobid=%s creamjobid=%s wn=%s status=%d\n",argv0,en.batch_id,en.user_prefix,en.wn_addr,en.status);
						}
						if (remupd_conf != NULL){
							if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
								do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in IntStateQuery\n",argv0);
							}
						}
					}
				}
			}
		
			freetoken(&token,maxtok_t);
			free(line);
			free(string_now);
			free(ren);
		}
		pclose(fp);
	}

	free(command_string);
	return 0;
}

int
FinalStateQuery(char *query)
{
/*
 Output format for status query for finished jobs for condor:
 batch_id   user      status     executable     exitcode   udate(timestamp_for_current_status)
 22018     gliteuser  4          /bin/sleep     0          1202288920

 Filled entries:
 batch_id
 status
 exitcode
 udate
 
 Filled by suhmit script:
 blah_id 
 
 Unfilled entries:
 wn_addr
 exitreason
*/
        FILE *fp;
	char *line=NULL;
	char **token;
	char **ctoken;
	int maxtok_t=0;
	int maxtok_c=0;
	job_registry_entry en;
	int ret=0;
	char *cp=NULL; 
	char *command_string=NULL;
	time_t now;
	char *string_now=NULL;
	int k=0;

	maxtok_c = strtoken(query, ';', &ctoken);

	for(k=0;k<maxtok_c;k++){
		command_string=make_message("%s%s/condor_history -constraint \"%s\" -format \"%%d \" ClusterId -format \"%%s \" Owner -format \"%%d \" JobStatus -format \"%%s \" Cmd -format \"%%s \" ExitStatus -format \"%%s\\n\" EnteredCurrentStatus",batch_command,condor_binpath,ctoken[k]);
		do_log(debuglogfile, debug, 2, "%s: command_string in FinalStateQuery:%s\n",argv0,command_string);
		fp = popen(command_string,"r");

		if(fp!=NULL){
			while(!feof(fp) && (line=get_line(fp))){
				do_log(debuglogfile, debug, 3, "%s: Line in FSQ:%s\n",argv0,line);
				if(line && (strlen(line)==0 || strncmp(line,"JOBID",5)==0)){
					free(line);
					continue;
				}
				if ((cp = strrchr (line, '\n')) != NULL){
					*cp = '\0';
				}
				
				maxtok_t = strtoken(line, ' ', &token);
				if (maxtok_t < 6){
					freetoken(&token,maxtok_t);
					free(line);
					continue;
				}
				
				now=time(0);
				string_now=make_message("%d",now);
			
				JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,token[0]);
				JOB_REGISTRY_ASSIGN_ENTRY(en.updater_info,string_now);
				en.status=atoi(token[2]);
				en.exitcode=atoi(token[4]);
				en.udate=atoi(token[5]);
		        	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
		        	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
			
				if(en.status!=UNDEFINED && en.status!=IDLE){	
					if ((ret=job_registry_update(rha, &en)) < 0){
						if(ret != JOB_REGISTRY_NOT_FOUND){
							fprintf(stderr,"Update of record returns %d: ",ret);
							perror("");
						}
					} else {
						do_log(debuglogfile, debug, 2, "%s: registry update in FinalStateQuery for: jobid=%s creamjobid=%s wn=%s status=%d\n",argv0,en.batch_id,en.user_prefix,en.wn_addr,en.status);
						if (en.status == REMOVED || en.status == COMPLETED){
							job_registry_unlink_proxy(rha, &en);
						}
						if (remupd_conf != NULL){
							if (ret=job_registry_send_update(remupd_head_send,&en,NULL,NULL)<=0){
								do_log(debuglogfile, debug, 2, "%s: Error creating endpoint in FinalStateQuery\n",argv0);
							}
						}
					}
				}
				freetoken(&token,maxtok_t);
				free(string_now);
				free(line);
			}
			pclose(fp);
		}

		free(command_string);
	}
	freetoken(&ctoken,maxtok_c);
	return 0;
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

int GetCondorVersion(){

        char *command_string=NULL;
        FILE *fp;
        char *line=NULL;
        char **token;
        int maxtok_t=0;
        char *condor_version=NULL;
        int c_version=0;
        char *cp=NULL;

	command_string=make_message("%s/condor_version",condor_binpath);
        fp = popen(command_string,"r");

        if(fp!=NULL){
                while(!feof(fp) && (line=get_line(fp))){
                        if ((cp = strrchr (line, '\n')) != NULL){
                                *cp = '\0';
                        }

                        if(line && strstr(line,"CondorVersion")!=0){
                                maxtok_t = strtoken(line, ' ', &token);
                                condor_version=strdel(token[1],".");
                                freetoken(&token,maxtok_t);
                                break;
                        }
                }
        }
        c_version=atoi(condor_version);
        return c_version;
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
	printf("Usage: BUpdaterCondor [OPTION...]\n");
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
	printf("Usage: BUpdaterCondor [-ov?] [-o|--nodaemon] [-v|--version] [-?|--help]\n");
	printf("        [--usage]\n");
	exit(EXIT_SUCCESS);
}
