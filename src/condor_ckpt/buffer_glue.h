
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "buffer_cache.h"
#include "file_state.h"

#ifndef BUFFER_GLUE_H
#define BUFFER_GLUE_H

/**
The BufferCache object was designed to fit nicely in
the new-syscalls-branch.  This module exists to graft the
buffer cache object into the current trunk until new-syscalls
is brought into the trunk.

WARNING WARNING WARNING:

This hack is completely incompatible with dup()'d files and files
that have been opened twice in the same table.  Some checking of
this condition has been built in to the file table so that some 
egregious buffering mistakes will not be made.

*/

/** Configure the buffer according to the shadow's instructions.
 ** If the buffer is already configured, return without harm. */
void BufferGlueConfigure();

/** Returns true if the buffer is active and the file in question
 ** should be buffered. */
int BufferGlueActive( File *f );

/** Perform a buffered read on this file object **/
ssize_t BufferGlueRead( File *f, char *data, size_t length );

/** Perform a buffered write on this file object **/
ssize_t BufferGlueWrite( File *f, char *data, size_t length );

/** Perform a buffered seek on this file object **/
off_t BufferGlueSeek( File *f, off_t offset, int whence );

/** Perform any special buffer tasks after file is opened */
void BufferGlueOpenHook( File *f );

/** Perform any special buffer tasks after file is closed */
void BufferGlueCloseHook( File *f );

extern "C" void _condor_file_warning( char *format, ... );

#endif /* BUFFER_GLUE_H */
