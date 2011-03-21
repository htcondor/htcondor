/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


#include <iostream.h>
#include <stdlib.h>
#include <fstream.h>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <map.h>
#include <string.h>

extern const char* eventNames[]={"job_submit","job_execute","job_executable_error","job_checkpointed","job_evicted","job_terminated","job_image_size","ulog_shadow_exception","ulog_generic","job aborted","job suspended","job_unsuspended","job held","job released","ulog_node_execute","ulog_node_terminated","ulog_post_script_terminated","job_globus_submit","job_globus_submit_failed","ulog_globus_resource_up"," ulog_globus_resource_down","ulog_remote_error","job_disconnected"," job_reconnected","job_reconnect_failed","ulog_grid_resource_up","ulog_grid_resource_down","ulog_grid_submit","job_ad_information"};
/*extern int reportEventId[]={0,1};  
extern int reject_list_size =2;
extern int max_file_size = 2000000;
//int max_file_size = 2000;
extern char* AttrList_toAvoid[]={"MyType","TargetType","EventTypeNumber","EventTime"};
extern char * mytype = "MyType";
extern int attr_limit = 4;
extern int max_event_id = 28;
extern char* jobid =" job.id=";
extern char* guidstr = " guid=";
extern char* schedName = "SCHED1";
*/
/*int ULogEventOutcomeIndex[]= {
 ULOG_OK = 0,
 ULOG_NO_EVENT = 1,
 ULOG_RD_ERROR = 2,
 ULOG_MISSED_EVENT = 3,
 ULOG_UNK_ERROR = 4
};
*/

/*extern const char* ULogEventOutcomeName[] = {
 "ULOG_OK       ",
 "ULOG_NO_EVENT ",
 "ULOG_RD_ERROR ",
 "ULOG_MISSED_EVENT ",
 "ULOG_UNK_ERROR"
};
*/
/*typedef map<string,string> StringIntMap;
  StringIntMap ahash;
  ahash["MyType"] = " event.name=condor.";
  ahash["EventTime"]= "ts=";
  ahash["EventTypeNumber"] = " event.id=";
  ahash["TargetType"] = "";
  */
