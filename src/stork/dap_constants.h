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

#ifndef _DAP_CONSTANTS_H_
#define _DAP_CONSTANTS_H_

#define MAXSTR          1024               // max. string size
#define BUF_SIZE        1024*1024          //buffer size for read & write

//--- name of directory where a catalog DaP jobs reside ---
//#define DAP_CATALOG    "DaP_Catalog"
//#define STORK_CONFIG_FILE "stork.config"

//--------- some SRB constants (for linked version only) ------------
#define srbAuth		NULL
#define HOST_ADDR	NULL
#define SRB_PORT        NULL    
#define ROWS_WANTED     1

//#define SRB_PORT        "5832"   
//#define COLLECTION      "/home/thh.caltech/USNOB/000" 

#endif

