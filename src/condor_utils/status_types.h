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

#ifndef __STATUS_TYPES_H__
#define __STATUS_TYPES_H__

// pretty-printing options
enum ppOption {
    PP_NOTSET,
	
	PP_DEFRAG_NORMAL,
    PP_STARTD_NORMAL,
    PP_STARTD_SERVER,
    PP_STARTD_STATE,
    PP_STARTD_RUN,
    PP_STARTD_COD,

    PP_QUILL_NORMAL,

    PP_SCHEDD_NORMAL,
    PP_SCHEDD_SUBMITTORS,	

    PP_MASTER_NORMAL,
    PP_COLLECTOR_NORMAL,
    PP_CKPT_SRVR_NORMAL,
    PP_GRID_NORMAL,
	PP_STORAGE_NORMAL,
    PP_NEGOTIATOR_NORMAL,
    PP_ANY_NORMAL,
    PP_GENERIC_NORMAL,
    PP_GENERIC,
    PP_XML,
    PP_CUSTOM,

    PP_VERBOSE
};


// modes for condor_status
enum Mode {
    MODE_NOTSET,
	MODE_DEFRAG_NORMAL,
    MODE_STARTD_NORMAL,
    MODE_STARTD_AVAIL,
    MODE_STARTD_RUN,
    MODE_STARTD_COD,
    MODE_QUILL_NORMAL,
    MODE_SCHEDD_NORMAL,
    MODE_SCHEDD_SUBMITTORS,
    MODE_MASTER_NORMAL,
    MODE_COLLECTOR_NORMAL,
    MODE_CKPT_SRVR_NORMAL,
    MODE_GRID_NORMAL,
    MODE_LICENSE_NORMAL,
    MODE_STORAGE_NORMAL,
    MODE_NEGOTIATOR_NORMAL,
    MODE_GENERIC_NORMAL,
    MODE_ANY_NORMAL,
    MODE_OTHER,
    MODE_GENERIC,
    MODE_HAD_NORMAL
};
   
#endif //__STATUS_TYPES_H__


