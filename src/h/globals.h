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
