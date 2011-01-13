/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_performance_monitor.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

#define CONDOR 1
#define STORK  2

int logtype;
char Node;
char terminatednormally_true[MAXSTR];
char jobcompletedevent_true[MAXSTR];

float dailychart[7*1440];
float dailychart_A[7*1440];
float dailychart_C[7*1440];

int concurrency[7*1440];
int concurrency_A[7*1440];
int concurrency_C[7*1440];

int getmonth(char *monthstr)
{
  if (!strcmp(monthstr, "Jan"))
    return 1;
  else if (!strcmp(monthstr, "Feb"))
    return 2;
  else if (!strcmp(monthstr, "Mar"))
    return 3;
  else if (!strcmp(monthstr, "Apr"))
    return 4;
  else if (!strcmp(monthstr, "May"))
    return 5;
  else if (!strcmp(monthstr, "Jun"))
    return 6;
  else if (!strcmp(monthstr, "Jul"))
    return 7;
  else if (!strcmp(monthstr, "Aug"))
    return 8;
  else if (!strcmp(monthstr, "Sep"))
    return 9;
  else if (!strcmp(monthstr, "Oct"))
    return 10;
  else if (!strcmp(monthstr, "Nov"))
    return 11;
  else if (!strcmp(monthstr, "Dec"))
    return 12;

    return -1;	// stops compiler warnings
}


int getValue(classad::ClassAd *currentAd, char *attr, char *attrval)
{
  std::string adbuffer = "";
  classad::ClassAdXMLUnParser unparser;
  classad::ExprTree *attrexpr = NULL;

  if (!(attrexpr = currentAd->Lookup(attr)) ){
    //printf("%s not found!\n", attr);
    strncpy(attrval, "", 1024);
    return DAP_ERROR;
  }

  unparser.Unparse(adbuffer, attrexpr);
  strncpy(attrval, adbuffer.c_str(), 1024);

  //if (attrexpr != NULL) delete attrexpr; attrexpr = NULL;
  return DAP_SUCCESS;
}

int main(int argc, char* argv[])
{
  int fd;
  FILE *fp, *fout, *foutA, *foutC;
  std::string mystr;

  bool job_completed = false;
  int jobs_succeeded = 0, jobs_failed = 0, jobs_unfinished = 0;
  int timeinseconds = 0, firstday = 0;
  long epocseconds;
  
  if (argc <4){
    printf("Usage: %s <logfile> <logtype> <outfile>\n", argv[0]);
    exit (1);
  }
  logtype = atoi(argv[2]);

  long i; 

  //initialization
  for (i=0;i<7*1440;i++){
    dailychart[i] = 0;
    dailychart_A[i] = 0;
    dailychart_C[i] = 0;

    concurrency[i] = 0;
    concurrency_A[i] = 0;
    concurrency_C[i] = 0;
  }
  
  
  if (logtype == CONDOR){
    strcpy(terminatednormally_true, "<b v=\"t\"/>");
    strcpy(jobcompletedevent_true, "<s>JobTerminatedEvent</s>");
  }
  else{//if STORK
    strcpy(terminatednormally_true, "<n>1</n>");\
    strcpy(jobcompletedevent_true, "<s>JobCompleteEvent</s>");
  }
  
    
  if( (fd = open( argv[1], O_RDWR, 0 )) == -1 ) {
    printf("Error in opening file : %s\n", argv[1]);
    return DAP_ERROR;
  }
  else if ((fp = fdopen (fd, "r")) == NULL) return DAP_ERROR;
  
  if (logtype == CONDOR){
    char fname_A[MAXSTR], fname_C[MAXSTR];
    sprintf(fname_A,"%s_A",argv[3]);
    sprintf(fname_C,"%s_C",argv[3]);

    if ( (foutA = safe_fopen_wrapper((fname_A, "w")) == NULL ){
      printf("Error in creating file : %s\n", fname_A);
      return DAP_ERROR;
    }
    if ( (foutC = safe_fopen_wrapper((fname_C, "w")) == NULL ){
      printf("Error in creating file : %s\n", fname_C);
      return DAP_ERROR;
    }
  }
  else{ //logtype == STORK
    if ( (fout = safe_fopen_wrapper((argv[3], "w")) == NULL ){
    printf("Error in creating file : %s\n", argv[3]);
    return DAP_ERROR;
    }
  }
  

  classad::ClassAd *newAd = 0;
  classad::ClassAdXMLParser xmlp;
  classad::ClassAdXMLUnParser unparser;
  char mytype[MAXSTR], cluster[MAXSTR], lognotes[MAXSTR];
  char terminatednormally[MAXSTR], returnvalue[MAXSTR];

  char eventtime1[MAXSTR]; //time of execute event
  int year = 0, month = 0, day, hour, minute, second;
  char daystr[MAXSTR], monthstr[MAXSTR];
  int dateinminutes;
  //  long dateinseconds;

  char mytype2[MAXSTR], cluster2[MAXSTR];
  char terminatednormally2[MAXSTR], returnvalue2[MAXSTR];
  char eventtime2[MAXSTR]; //time of terminate event
  int year2 = 0 , month2 = 0, day2, hour2, minute2, second2;
  char daystr2[MAXSTR], monthstr2[MAXSTR];
  int dateinminutes2;
  // long dateinseconds2;
  
  fpos_t *lastpos;
  lastpos = (fpos_t*)malloc(sizeof(fpos_t));

  int job_count = 0;
  while (1){

    if (job_count++ >= 1500) break;
    
    newAd = xmlp.ParseClassAd(fp);

    if (feof(fp)) break;

    getValue(newAd, "MyType", mytype);
    getValue(newAd, "Cluster", cluster);
    getValue(newAd, "LogNotes", lognotes);
    getValue(newAd, "TerminatedNormally", terminatednormally);
    getValue(newAd, "ReturnValue", returnvalue);

    /*
    getValue(newAd, "EventTime", eventtime);

    if (logtype == CONDOR){
      sscanf(eventtime, "<s>%d-%d-%dT%d:%d:%d</s>", 
	     &year, &month, &day, &hour, &minute, &second);
    }
    else{
      sscanf(eventtime, "<s>%s %s %d %d:%d:%d %d </s>", 
	     monthstr, daystr, &day, &hour, &minute, &second, &year);
    }
    */
    
    /*
    printf("mytype = %s\n", mytype);
    printf("cluster = %s\n", cluster);
    printf("lognotes = %s\n", lognotes);
    printf("terminatednormally = %s\n", terminatednormally);
    printf("returnvalue = %s\n", returnvalue);
    //    printf("eventtime = %s\n", eventtime);
    printf("Date: %d-%d-%d %d:%d:%d\n", year, month, day, hour, minute, second);
    */


    if (!strcmp(mytype, "<s>SubmitEvent</s>")){
      //printf("This is a submit event\n");
      fgetpos(fp, lastpos);
      
      while(1){

	if (newAd != NULL) delete newAd; newAd = NULL;	
	newAd = xmlp.ParseClassAd(fp);
	if (feof(fp)) break;

	getValue(newAd, "Cluster", cluster2);
	if (!strcmp(cluster, cluster2)){
	  getValue(newAd, "MyType", mytype2);
	  getValue(newAd, "TerminatedNormally", terminatednormally2);
	  getValue(newAd, "ReturnValue", returnvalue2);

	  if (!strcmp(mytype2, "<s>ExecuteEvent</s>")){
	    getValue(newAd, "EventTime", eventtime1);
	    if (logtype == CONDOR){
	      sscanf(eventtime1, "<s>%d-%d-%dT%d:%d:%d</s>",
		     &year, &month, &day, &hour, &minute, &second);
	    }
	    else{
	      sscanf(eventtime1, "<s>%s %s %d %d:%d:%d %d </s>",
		     daystr, monthstr, &day, &hour, &minute, &second, &year);
	      month = getmonth(monthstr);
	    }
	  }
	  if (firstday == 0) {
	    firstday = day;
	    epocseconds = (year - 1970) * 365*24*60*60 
	      + (month - 1) * 30*24*60*60
	      + (day - 1) * 24*60*60
	      + (9 ) * 24*60*60 //days
	      + (5) * 60*60; //hours

	  }
    
	  
	  if (!strcmp(mytype2, jobcompletedevent_true)){
	    if ((!strcmp(terminatednormally2,terminatednormally_true))
		&&(!strcmp(returnvalue2, "<n>0</n>")))
	      {
		/*
		if ((Node == 'A') || (Node == 'B')){
		  printf("Job %s completed successfully", cluster);
		}
		*/

		getValue(newAd, "EventTime", eventtime2);

		if (logtype == CONDOR){
		  sscanf(eventtime2, "<s>%d-%d-%dT%d:%d:%d</s>", 
			 &year2, &month2, &day2, &hour2, &minute2, &second2);
		  sscanf(lognotes, "<s>DAG Node: %c", &Node);
		  //printf("Node: %c\n", Node);
		  
		}
		else{
		  sscanf(eventtime2, "<s>%s %s %d %d:%d:%d %d (CST) </s>", 
			 monthstr2, daystr2, &day2, &hour2, &minute2, 
			 &second2, &year2);
		}
		
		timeinseconds = (second2 - second) + 60 * (minute2 - minute) + 60*60 * (hour2 - hour) + 24*60*60 * (day2 - day) + 365*24*60*60 * (year2-year);
		
		/*
		if ((Node == 'A') || (Node == 'B')){
		  printf(" in %d seconds.\n", timeinseconds);
		}
		*/
		
		//printf("transfer rate: %f MB/sec, %f MB/min\n", (float)1058/timeinseconds, (float)60*1058/timeinseconds);
		
		dateinminutes = (day-firstday)*1440 + 60*hour + minute;
		//		dateinseconds = (day-firstday)*1440*60 + 60*60*hour +   60*minute + second;
		//	if (second > 30) dateinminutes++;
		
		dateinminutes2 = (day2-firstday)*1440 + 60*hour2 + minute2;
		//		dateinseconds2 = (day2-firstday)*1440*60 + 60*60*hour2 +  60*minute2 + second2; 
		
		//if (second2 > 30) dateinminutes2 ++;

		for (i=dateinminutes; i<dateinminutes2; i++){
		  if (logtype == CONDOR){
		    if (Node == 'A'){
		      dailychart_A[i] += (float)1058/timeinseconds;
		      concurrency_A[i] += 1;
		    }
		    else if (Node == 'C'){
		      dailychart_C[i] += (float)1058/timeinseconds;
		      concurrency_C[i] += 1;
		    }
		    
		  }
		  else{
		    dailychart[i] += (float)1058/timeinseconds;
		    concurrency[i] += 1;
		  }
		  
		}
		
		//printf("dateinminutes1 = %d, dateinminutes2 = %d\n",dateinminutes, dateinminutes2);
		//printf("Date1: %d-%d-%d %d:%d:%d\n", year, month, day, hour, minute, second);
		//printf("Date2: %d-%d-%d %d:%d:%d\n\n", year2, month2, day2, hour2, minute2, second2);
		

		
		jobs_succeeded ++;
		job_completed = true;
		break;
		
	      }//if job succeeded
	    else
	      {
		printf("job %s failed!\n", cluster);
		jobs_failed ++;
		job_completed = true;
		break;
		
	      }//if job failed

	  }//if job completed
	}//if another event for the same job
      }// while -> search for other events for the same job
      
      if (!job_completed){
	printf("job %s not completed!\n", cluster);
	jobs_unfinished ++;
      }

      fsetpos(fp, lastpos);
    }//if it is a submit event

    /*
    mystr = "";
    unparser.Unparse(mystr, newAd);
    printf("New Ad => %s\n",mystr.c_str());
    */
    if (newAd != NULL) delete newAd; newAd = NULL;
    
  }//search for all events
  
  printf("Search complete!\n");
  

  long j = 1, ja = 1, jc = 1;

  for (i=0;i<7*1440;i++){
    if (dailychart_C[i] > 12.5) dailychart_C[i] = 12.5;
  }//lame tune up for 100Mb/sec interface card
  
  for (i=0;i<7*1440;i++){
    if (logtype == CONDOR){
      if (dailychart_A[i] > 0){
	fprintf(foutA, "%ld %ld %f %d\n",ja, epocseconds + i*60, 
		dailychart_A[i], concurrency_A[i]);
	ja++;	
      }
      
      if (dailychart_C[i] > 0){
	fprintf(foutC, "%ld %ld %f %d\n",jc, epocseconds + i*60, 
		dailychart_C[i], concurrency_C[i]);
	jc++;	
      }
    }
    else{ //logtype == STORK
      if (dailychart[i] > 0){
	fprintf(fout, "%ld %ld %f %d\n",j, epocseconds + i*60, 
		dailychart[i], concurrency[i]);
	j++;
      }
    }
    
  }

  //print report
  printf("==========================\n");
  printf("# of jobs succeeded  : %d\n", jobs_succeeded);
  printf("# of jobs failed     : %d\n", jobs_failed);
  printf("# of jobs unfinished : %d\n", jobs_unfinished);
  printf("==========================\n");
  
}












