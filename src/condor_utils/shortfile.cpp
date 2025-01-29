#include "condor_common.h"
#include "condor_debug.h"

#include <string>

#include "shortfile.h"
#include "safe_open.h"


//
// Utility function; inefficient.
// FIXME: GT #3924.
//
bool
htcondor::readShortFile( const std::string & fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_RDONLY | _O_BINARY, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName.c_str(), strerror( errno ), errno );
        return false;
    }

    struct stat statbuf = {};
    fstat(fd, &statbuf);
    filesize_t fileSize = statbuf.st_size;

    char * rawBuffer = (char *)malloc( fileSize + 1 );
    assert( rawBuffer != NULL );
    filesize_t totalRead = full_read( fd, rawBuffer, fileSize );
    close( fd );
    if( totalRead != fileSize ) {
        dprintf( D_ALWAYS, "Failed to completely read file '%s'; needed %lld but got %lld.\n",
		         fileName.c_str(), (long long)fileSize, (long long)totalRead );
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
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC | _O_BINARY, 0600 );

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
htcondor::writeShortFile(
    const std::string & fileName,
    unsigned char * bytes,
    size_t count
) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC | _O_BINARY, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for writing: '%s' (%d).\n", fileName.c_str(), strerror( errno ), errno );
        return false;
    }

    unsigned long written = full_write( fd, bytes, count );
    close( fd );
    if( written != count ) {
        dprintf( D_ALWAYS, "Failed to completely write file '%s'; wanted to write %zu but only put %zu.\n",
                 fileName.c_str(), count, written );
        return false;
    }

    return true;
}


bool
htcondor::appendShortFile( const std::string & fileName, const std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_APPEND | _O_BINARY, 0600 );

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
