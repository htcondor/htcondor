/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __STATUS_TYPES_H__
#define __STATUS_TYPES_H__

// pretty-printing options
enum ppOption {
    PP_NOTSET,

    PP_STARTD_NORMAL,
    PP_STARTD_SERVER,
	PP_STARTD_STATE,
    PP_STARTD_RUN,
    PP_STARTD_COD,

	PP_SCHEDD_NORMAL,
	PP_SCHEDD_SUBMITTORS,	

	PP_MASTER_NORMAL,
	PP_COLLECTOR_NORMAL,
	PP_CKPT_SRVR_NORMAL,
	PP_STORAGE_NORMAL,
	PP_ANY_NORMAL,
    PP_VERBOSE,
    PP_XML,
    PP_CUSTOM
};


// modes for condor_status
enum Mode {
    MODE_NOTSET,
    MODE_STARTD_NORMAL,
    MODE_STARTD_AVAIL,
    MODE_STARTD_RUN,
    MODE_STARTD_COD,
	MODE_SCHEDD_NORMAL,
	MODE_SCHEDD_SUBMITTORS,
	MODE_MASTER_NORMAL,
	MODE_COLLECTOR_NORMAL,
	MODE_CKPT_SRVR_NORMAL,
	MODE_LICENSE_NORMAL,
	MODE_STORAGE_NORMAL,
	MODE_ANY_NORMAL
};
   
#endif //__STATUS_TYPES_H__


