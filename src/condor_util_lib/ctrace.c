/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
