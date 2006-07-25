/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

