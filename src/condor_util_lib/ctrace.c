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

#include "condor_common.h" 
#include "ctrace.h"
#include "globals.h"

TraceEntry entry;
long       traceFlag         = 0;
long       numberOfTransfers = 0;

void StartRecording()
  {
    if ((traceFlag) || (numberOfTransfers == 0))
    {
      memset(&entry,0, sizeof(TraceEntry)); /* dhaval 9/25 */
      gettimeofday(&entry.initialTime, 0);
    }
  }

void CompleteRecording(numberOfBytes)
  long numberOfBytes;
  {
    if ((traceFlag) || (numberOfTransfers == 0))
    {
      gettimeofday(&entry.completionTime, 0);
      entry.operation     = TRANSFER;
      entry.numberOfBytes = numberOfBytes;
      write(2 /* stderr */, &entry, sizeof(TraceEntry));
    }
    numberOfTransfers++;
  }

void ProcessLogging (request, extraInteger)
  int request;
  int extraInteger;
  {
    switch (request)
    {
      case ENABLE_LOGGING :   traceFlag = 1;
                              if (numberOfTransfers == 1)
                              {
                                write(2 /*stderr*/, &entry, sizeof(TraceEntry));
                              }
                              break;
      case DISABLE_LOGGING  : traceFlag = 0;
                              break;
      case START_LOGGING    : 
                              StartRecording();
                              break;
      case COMPLETE_LOGGING : 
			      if (traceFlag)
			      {
				gettimeofday(&entry.completionTime, 0);
				entry.operation     = SENDBACK; 
				entry.numberOfBytes = extraInteger;
				write(2 /*stderr*/, &entry, sizeof(TraceEntry));
			      }
			      numberOfTransfers++;
                              break;
      case CATCH_SIGKILL    : 
			      if (traceFlag)
			      {
				memset(&entry, 0,sizeof(TraceEntry));/* dhaval 9/25*/
				gettimeofday(&entry.initialTime, 0);
				entry.operation = INTERRUPT; 
				write(2 /*stderr*/, &entry, sizeof(TraceEntry));
			      }
			      numberOfTransfers++;
			      break;
    }
  }
