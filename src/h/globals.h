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
