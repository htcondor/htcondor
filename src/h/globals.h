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

 


#define OPEN        1      /* IO operation */
#define WRITE       2      /* IO operation */
#define READ        3      /* IO operation */
#define LSEEK       4      /* IO operation */
#define CLOSE       5      /* IO operation */
#define FLUSH       6      /* IO operation */
#define START       7      /* IO operation */
#define CHECKPOINT  8      /* IO operation */
#define INTERRUPT   9      /* IO operation */
#define TRANSFER   10      /* IO operation */
#define EXIT       11      /* IO operation */
#define SENDBACK   12      /* IO operation */

#define MAX_FILES  20

typedef struct
  {
    unsigned char  operation;      /* One of the "defines" given above        */
    struct timeval initialTime;    /* Taken at the beginning of the operation */
    struct timeval completionTime; /* Time taken at the end of the operation  */
    int            fileDescriptor; /* On which the operation was made         */
    long           numberOfBytes;  /* # of bytes involved in the IO operation */
  }
TraceEntry;
