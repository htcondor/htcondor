#include "condor_common.h"
#include "condor_debug.h"

#include <string>

#include "shortfile.h"
#include "safe_open.h"
#include "stat_wrapper.h"

//
// Utility function; inefficient.
// FIXME: GT #3924.
//
bool
htcondor::readShortFile( const std::string & fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_RDONLY, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName.c_str(), strerror( errno ), errno );
        return false;
    }

    StatWrapper sw( fd );
    unsigned long fileSize = sw.GetBuf()->st_size;

    char * rawBuffer = (char *)malloc( fileSize + 1 );
    assert( rawBuffer != NULL );
    unsigned long totalRead = full_read( fd, rawBuffer, fileSize );
    close( fd );
    if( totalRead != fileSize ) {
        dprintf( D_ALWAYS, "Failed to completely read file '%s'; needed %lu but got %lu.\n",
            fileName.c_str(), fileSize, totalRead );
        free( rawBuffer );
        return false;
    }
    contents.assign( rawBuffer, fileSize );
    free( rawBuffer );

    return true;
}

//
// Utility function.
//
bool
htcondor::writeShortFile( const std::string & fileName, const std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for writing: '%s' (%d).\n", fileName.c_str(), strerror( errno ), errno );
        return false;
    }

    unsigned long written = full_write( fd, contents.c_str(), contents.length() );
    close( fd );
    if( written != contents.length() ) {
        dprintf( D_ALWAYS, "Failed to completely write file '%s'; wanted to write %lu but only put %lu.\n",
                 fileName.c_str(), (unsigned long)contents.length(), written );
        return false;
    }

    return true;
}

bool
htcondor::appendShortFile( const std::string & fileName, const std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_APPEND, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for writing: '%s' (%d).\n", fileName.c_str(), strerror( errno ), errno );
        return false;
    }

    unsigned long written = full_write( fd, contents.c_str(), contents.length() );
    close( fd );
    if( written != contents.length() ) {
        dprintf( D_ALWAYS, "Failed to completely append to file '%s'; wanted to append %lu but only put %lu.\n",
                 fileName.c_str(), (unsigned long)contents.length(), written );
        return false;
    }

    return true;
}
