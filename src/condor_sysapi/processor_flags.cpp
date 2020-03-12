#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "sysapi.h"
#include "sysapi_externs.h"

/*
 * See GitTrac #3544 for an explanation of why we care about processor flags
 * (short answer: because glibc does, and breaks standard universe as a 
 * result).  To determine which flags glibc cares about, examine the glibc
 * sources; in 2.12-2, the file sysdeps/x86_64/multiarch/init-arch.h #defines
 * [bit|index]_[SSE2|SSSE3|SSE4_1|SSE4_2].  I confirmed (by checking with
 * Wikipedia's CPUID article) that glibc was using the same names for those
 * processor flags as everyone else (using the value of the shift in the bit_*
 * macros, combined with CPUID_E[C|D]X_OFFSET).  Thus, SSSE3 really does have
 * three 'S's.
 *
 * We ignore what glibc is doing in init-arch.c because it's strictly for
 * performance -- see the ticket for details.
 *
 * We're ignoring SSE2 because every processor built in the last 10 years
 * supports it.
 *
 * We're also (implicitly) asserting homogeneous cores; we simply don't have
 * a way to represent anything else at this level of the code.  Instead, we
 * complain to the log if the processor flag strings aren't all the same.
 */

static struct sysapi_cpuinfo theInfo;

const struct sysapi_cpuinfo *sysapi_processor_flags_raw( void ) {
    sysapi_internal_reconfig();
    
    if( _sysapi_processor_flags_raw != NULL ) {
        return &theInfo;
    }

    /* Set the default to the empty string so that if something goes wrong
       during parsing (or if /proc/cpuinfo doesn't exist), we stop trying. */
    _sysapi_processor_flags_raw = "";

    /* If we check for the null string, we could leak memory for processors
       without flags.  (We shouldn't see any, but...) */
    int foundProcessorFlags = 0;
    
    /* You can adapt this to ncpus.cpp's _SysapiProcCpuinfo for debugging. */
    FILE * fp = safe_fopen_wrapper_follow( "/proc/cpuinfo", "r", 0644 );
    dprintf( D_LOAD, "Reading from /proc/cpuinfo\n" );
    if( fp ) {
        int size = 128;
        char * buffer = (char *)malloc( size );
        if( buffer == NULL ) {
            EXCEPT( "Failed to allocate buffer for parsing /proc/cpuinfo.\n" );
        }            
        
        while( fgets( buffer, size, fp ) ) {
            while( strchr( buffer, '\n' ) == NULL ) {
                char * newBuffer = (char *)realloc( buffer, size + size );
                if( newBuffer == NULL ) {
                    EXCEPT( "Failed to allocate memory for a long line in /proc/cpuinfo.\n" );
                }
                buffer = newBuffer;
                
                newBuffer = buffer + strlen( buffer );
                if( ! fgets( newBuffer, size, fp ) ) {
                    /* If we fail a read before finding the end of the line,
                       something has probably gone terribly, terribly wrong. */
                    EXCEPT( "Failed to find end of line ('%s') before end of file.\n", buffer );
                    // If /proc/cpuinfo regularly terminates without a newline,
                    // we could do this instead.
                    // free( buffer );
                    // fclose( fp );
                    // return _sysapi_processor_flags_raw;
                }
                size += size;
            }

            char * colon = strchr( buffer, ':' );
            
            const char * value = "";
            const char * attribute = NULL;
            if( colon != NULL ) {
                for( unsigned int v = 1; colon[v] != '\0' && isspace( colon[v] ); ++v ) {
                    value = colon + v;
                }
                
                char * tmp = colon;
                while( isspace( *tmp ) || (*tmp == ':' ) ) {
                    *tmp = '\0';
                    --tmp;
                }
                attribute = buffer;
                
                if( strcmp( attribute, "flags" ) == 0 ) {
                    if( foundProcessorFlags == 0 ) {
                        /* This is where we assume flags fits into buffer. */
                        _sysapi_processor_flags_raw = strdup( value );
                        if( _sysapi_processor_flags_raw == NULL ) {
                            EXCEPT( "Failed to allocate memory for the raw processor flags.\n" );
                        }
                    } else {
                        if( strcmp( _sysapi_processor_flags_raw, value ) != 0 ) {
                            dprintf( D_ALWAYS, "WARNING: Processor flags '%s' and '%s' are not the same; using the former.\n", _sysapi_processor_flags_raw, value );
                        }
                    }
                    
                    foundProcessorFlags += 1;
                } else if (strcmp(attribute, "model") == 0) {
			int tmp = 0;
			int r = sscanf(value, "%d", &tmp);
			if (r > 0) theInfo.model_no = tmp;
		} else if (strcmp(attribute,"cpu family") == 0) {
			int tmp = 0;
			int r = sscanf(value, "%d", &tmp);
			if (r > 0) theInfo.family = tmp;
		} else if (strcmp(attribute,"cache size") == 0) {
			int tmp = 0;
			int r = sscanf(value, "%d", &tmp);
			if (r > 0) theInfo.cache = tmp;
		}
            }
        }
        
        free( buffer );
        fclose( fp );
    }
    
    theInfo.processor_flags = _sysapi_processor_flags;
    return &theInfo;
}

const struct sysapi_cpuinfo *sysapi_processor_flags( void ) {
    sysapi_internal_reconfig();
    
    if( _sysapi_processor_flags != NULL ) {
        return &theInfo;
    }
    
    if( _sysapi_processor_flags_raw == NULL ) {
        sysapi_processor_flags_raw();
        ASSERT(_sysapi_processor_flags_raw != NULL);
    }

    /* Which flags do we care about?  You MUST terminate this list with NULL. */
    static const char * const flagNames[] = { "avx", "avx2", "avx512f", "avx512dq", "avx512_vnni", "ssse3", "sse4_1", "sse4_2", NULL };

    /* Do some memory-allocation math. */
    int numFlags = 0;
    int maxFlagLength = 0;
    for( int i = 0; flagNames[i] != NULL; ++i ) {
        ++numFlags;
        int curFlagLength = (int)strlen( flagNames[i] );
        if( curFlagLength > maxFlagLength ) { maxFlagLength = curFlagLength; }
    }

    char * currentFlag = (char *)malloc( (1 + maxFlagLength) * sizeof( char ) );
    if( currentFlag == NULL ) {
        EXCEPT( "Failed to allocate memory for current processor flag." );
    }        
    currentFlag[0] = '\0';

    /* If we track which flags we have, we can make sure the order we
       print them is the same regardless of the raw flags order. */
    const char ** flags = (const char **)malloc( sizeof( const  char * ) * numFlags );
    if( flags == NULL ) {
        EXCEPT( "Failed to allocate memory for processor flags." );
    }
    for( int i = 0; i < numFlags; ++i ) { flags[i] = ""; }

    const char * flagStart = _sysapi_processor_flags_raw;
    const char * flagEnd = _sysapi_processor_flags_raw;
    while( * flagStart != '\0' ) {
        if( * flagStart == ' ' ) { ++flagStart; continue; }

        for( flagEnd = flagStart; (* flagEnd != '\0') && (* flagEnd != ' '); ++flagEnd ) { ; }

        int flagSize = (flagEnd - flagStart) / (int)sizeof( char );
        if( flagSize > maxFlagLength ) {
            flagStart = flagEnd;
            continue;
        }
        
        /* We know that flagStart is neither ' ' nor '\0', so we must have
           at least one character.  Because we only care about flags of a
           certain length or smaller, we know we won't overflow our buffer. */
        strncpy( currentFlag, flagStart, flagSize );
        currentFlag[flagSize] = '\0';

        for( int i = 0; flagNames[i] != NULL; ++i ) {
            if( strcmp( currentFlag, flagNames[i] ) == 0 ) {
                /* Add to the flags. */
                flags[i] = flagNames[i];            
                break;
            }
        }

        flagStart = flagEnd;
    }
    free( currentFlag );

    /* How much space do we need? */
    int flagsLength = 1;
    for( int i = 0; i < numFlags; ++i ) {
        int length = (int)strlen( flags[i] );
        if( length ) { flagsLength += length + 1; }
    }
    
    if( flagsLength == 1 ) {
        _sysapi_processor_flags = "none";
    } else {
        char * processor_flags = (char *)malloc( sizeof( char ) * flagsLength );
        if( processor_flags == NULL ) {
            EXCEPT( "Failed to allocate memory for processor flag list." );
        }
        processor_flags[0] = '\0';

        /* This way, the flags will always print out in the same order. */
        for( int i = 0; i < numFlags; ++i ) {
            if( strlen( flags[i] ) ) {
                strcat( processor_flags, flags[i] );
                strcat( processor_flags, " " );
            }                    
        }

        /* Remove the trailing space. */
        processor_flags[ flagsLength - 2 ] = '\0';
        _sysapi_processor_flags = processor_flags;
    }
    
    free( flags );
    theInfo.processor_flags = _sysapi_processor_flags;
    return &theInfo;
}

// ----------------------------------------------------------------------------

#if defined( PROCESSOR_FLAGS_TESTING )

/* 
 * To compile:

g++ -DGLIBC=GLIBC -DHAVE_CONFIG_H -DLINUX -DPROCESSOR_FLAGS_TESTING \
    -I../condor_includes -I../condor_utils -I../safefile \
    -I<configured build directory>/src/condor_includes \
    processor_flags.cpp

 *
 */

// Required to link.
char * _sysapi_processor_flags = NULL;
char * _sysapi_processor_flags_raw = NULL;
void sysapi_internal_reconfig( void ) { ; }
int _EXCEPT_Line;
int _EXCEPT_Errno;
const char * _EXCEPT_File;
void _EXCEPT_ ( const char * fmt, ... ) { exit( 1 ); }

int main( int argc, char ** argv ) {
    const char * expected;
    
    expected = "sse4_1 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 sse4_1 sse3 ssse3 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "none";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "none";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"ssse3 sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 sse4_2 sse3 ssse3 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );
    
    return 0;
}

#endif
