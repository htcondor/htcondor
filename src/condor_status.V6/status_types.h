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

    PP_SLOTS_NORMAL,
    PP_SLOTS_SERVER,
    PP_SLOTS_STATE,
    PP_SLOTS_RUN,
    PP_SLOTS_COD,
    PP_SLOTS_GPUS,
    PP_SLOTS_BROKEN,

    PP_STARTDAEMON,
    PP_STARTD_GPUS,
    PP_STARTD_BROKEN,

    PP_SCHEDD_NORMAL,
    PP_SCHEDD_DATA,
    PP_SCHEDD_RUN,
    PP_SUBMITTER_NORMAL,

    PP_MASTER_NORMAL,
    PP_COLLECTOR_NORMAL,
    PP_CKPT_SRVR_NORMAL,
    PP_GRID_NORMAL,
    PP_STORAGE_NORMAL,
    PP_NEGOTIATOR_NORMAL,
    PP_DEFRAG_NORMAL,
    PP_ACCOUNTING_NORMAL,

    PP_ANY_NORMAL,
    PP_GENERIC_NORMAL,
    PP_GENERIC,
    PP_CUSTOM,

    PP_LONG,
    PP_XML,
    PP_JSON,
    PP_NEWCLASSAD, // output new classad format, ie [] around the ad and ; between attributes
};

#define PP_IS_LONGish(pp) ((pp) >= PP_LONG && (pp) <= PP_NEWCLASSAD)


// display modes for condor_status
enum {
	SDO_NotSet,			//  MODE_NOTSET,

	SDO_Slots,			//  MODE_SLOTS_NORMAL,
	SDO_Slots_Avail,	//  MODE_SLOTS_AVAIL,
	SDO_Slots_Claimed,	//  MODE_SLOTS_RUN,
	SDO_Slots_Cod,		//  MODE_SLOTS_COD,
	SDO_Slots_GPUs, 	//  MODE_SLOTS_GPUS,
	SDO_Slots_Broken, 	//  MODE_SLOTS_BROKEN,

	SDO_StartDaemon,    //  MODE_STARTD_DAEMON,
	SDO_StartD_GPUs,    //  MODE_STARTD_GPUS,
	SDO_StartD_Broken,  //  MODE_STARTD_BROKEN,

	SDO_Schedd,			//  MODE_SCHEDD_NORMAL,
	SDO_Schedd_Data,	//  MODE_SCHEDD_DATA,
	SDO_Schedd_Run,		//  MODE_SCHEDD_RUN,

	SDO_Submitters,		//  MODE_SCHEDD_SUBMITTORS,
	SDO_Master,			//  MODE_MASTER_NORMAL,
	SDO_Collector,		//  MODE_COLLECTOR_NORMAL,
	SDO_CkptSvr,		//  MODE_CKPT_SRVR_NORMAL,
	SDO_Grid,			//  MODE_GRID_NORMAL,
	SDO_License,		//  MODE_LICENSE_NORMAL,
	SDO_Storage,		//  MODE_STORAGE_NORMAL,
	SDO_Negotiator,		//  MODE_NEGOTIATOR_NORMAL,
	SDO_Defrag,			//  MODE_DEFRAG_NORMAL,
	SDO_Accounting,		//  
	SDO_Generic,		//  MODE_GENERIC_NORMAL,
	SDO_Any,			//  MODE_ANY_NORMAL,
	SDO_Other,			//  MODE_OTHER,
	SDO_HAD				//  MODE_HAD_NORMAL
};
   
#endif //__STATUS_TYPES_H__


