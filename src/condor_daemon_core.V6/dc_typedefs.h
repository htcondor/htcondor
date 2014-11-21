
#ifndef _DC_TYPEDEFS_H_
#define _DC_TYPEDEFS_H_

#include "dc_service.h"
#include "stream.h"

/** @name Typedefs for Callback Procedures
 */
//@{
///
typedef int     (*CommandHandler)(Service*,int,Stream*);

///
typedef int     (Service::*CommandHandlercpp)(int,Stream*);

///
typedef int     (*SignalHandler)(Service*,int);

///
typedef int     (Service::*SignalHandlercpp)(int);

///
typedef int     (*SocketHandler)(Service*,Stream*);

///
typedef int     (Service::*SocketHandlercpp)(Stream*);

///
typedef int     (*PipeHandler)(Service*,int);

///
typedef int     (Service::*PipeHandlercpp)(int);

///
typedef int     (*ReaperHandler)(Service*,int pid,int exit_status);

///
typedef int     (Service::*ReaperHandlercpp)(int pid,int exit_status);

///
typedef int             (*ThreadStartFunc)(void *,Stream*);

/// Register with RegisterTimeSkipCallback. Call when clock skips.  First
//variable is opaque data pointer passed to RegisterTimeSkipCallback.  Second
//variable is the _rough_ unexpected change in time (negative for backwards).
typedef void    (*TimeSkipFunc)(void *,int);

/** Does work in thread.  For Create_Thread_With_Data.
        @see Create_Thread_With_Data
*/

typedef int     (*DataThreadWorkerFunc)(int data_n1, int data_n2, void * data_vp);

/** Reports to parent when thread finishes.  For Create_Thread_With_Data.
        @see Create_Thread_With_Data
*/
typedef int     (*DataThreadReaperFunc)(int data_n1, int data_n2, void * data_vp, int exit_status);
//@}

typedef enum {
	HANDLE_NONE=0,
        HANDLE_READ,
        HANDLE_WRITE,
        HANDLE_READ_WRITE
} HandlerType;

#endif

