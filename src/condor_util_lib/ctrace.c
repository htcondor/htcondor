/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <sys/time.h>
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
