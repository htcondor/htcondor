#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "sysapi.h"
#include "sysapi_externs.h"
#include <algorithm>
#include <array>
#include <string>

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

#ifdef LINUX
static const struct sysapi_cpuinfo *sysapi_processor_flags_read_proc_cpuinfo( void ) {
    sysapi_internal_reconfig();
    
    /* Set the default to the empty string so that if something goes wrong
       during parsing (or if /proc/cpuinfo doesn't exist), we stop trying. */
    theInfo.processor_flags_full = "";

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
            EXCEPT( "Failed to allocate buffer for parsing /proc/cpuinfo." );
        }            
        
        while( fgets( buffer, size, fp ) ) {
            while( strchr( buffer, '\n' ) == NULL ) {
                char * newBuffer = (char *)realloc( buffer, size + size );
                if( newBuffer == NULL ) {
                    EXCEPT( "Failed to allocate memory for a long line in /proc/cpuinfo." );
                }
                buffer = newBuffer;
                
                newBuffer = buffer + strlen( buffer );
                if( ! fgets( newBuffer, size, fp ) ) {
                    /* If we fail a read before finding the end of the line,
                       something has probably gone terribly, terribly wrong. */
                    EXCEPT( "Failed to find end of line ('%s') before end of file.", buffer );
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
                        theInfo.processor_flags_full = value;
                    } else {
                        if( theInfo.processor_flags_full != value) {
                            dprintf( D_ALWAYS, "WARNING: Processor flags '%s' and '%s' are not the same; using the former.\n", theInfo.processor_flags_full.c_str(), value );
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
    
    return &theInfo;
}

#endif

#ifdef LINUX
// Turn a string of words separated by exactly one space
// into a vector of words and return int.
static std::vector<std::string>
split_string_to_vector(const char *input) {
	std::vector<std::string> result;
	
	const char *first = input;
	const char *last  = input;

	while (*last) {
		if (*last == ' ') {
			result.emplace_back(first, last);
			first = last + 1; // skip over space
		}
		last++;
	}
	result.emplace_back(first, last);

	return result;
}

// Turn a vector of std::strings into one space-separated string
static std::string
join_vector_to_string(const std::vector<std::string> &input) {
	std::string result;
	bool first = true;
	for (const auto &element : input) {
		if (first) {
			first = false;
			result += element;
		} else {
			result += ' ';
			result += element;
		}
	}
	return result;
}

static std::string
cpuflags_to_microarch_string(std::vector<std::string> &cpuinfoFlags) {
#ifdef X86_64
	// Defined in https://en.wikipedia.org/wiki/x86-64#Microarchitecture_levels
	//
	// These must be sorted!
	static const std::array<const std::string, 6> v2insns = { "cx16", "lahf_lm", "popcnt", "sse4_1", "sse4_2", "ssse3"};
	static const std::array<const std::string, 9> v3insns = { "abm", "avx", "avx2", "bmi1", "bmi2", "f16c", "fma", "movbe", "xsave"};
	static const std::array<const std::string, 5> v4insns = { "avx512bw", "avx512cd", "avx512dq", "avx512f", "avx512vl"};

	if (std::includes(cpuinfoFlags.begin(), cpuinfoFlags.end(), v4insns.begin(), v4insns.end())) {
		return "x86_64-v4";
	}

	if (std::includes(cpuinfoFlags.begin(), cpuinfoFlags.end(), v3insns.begin(), v3insns.end())) {
		return "x86_64-v3";
	}

	if (std::includes(cpuinfoFlags.begin(), cpuinfoFlags.end(), v2insns.begin(), v2insns.end())) {
		return "x86_64-v2";
	}

	return "x86_64-v1"; // Must be ancient
#else
	(void)cpuinfoFlags; // Shut the compiler up
	return ""; // only x86_64 defines microarch today
#endif
}

#endif

const struct sysapi_cpuinfo *sysapi_processor_flags( void ) {

	sysapi_internal_reconfig();

	// We've been here before
	if (theInfo.initialized) {
		return &theInfo;
	}

#ifdef LINUX
	sysapi_processor_flags_read_proc_cpuinfo();

	/* Which flags do we care about? MUST BE SORTED! */ 
	static const std::array<const std::string, 8> interestingFlags = 
	{ "avx", "avx2", "avx512_vnni","avx512dq", "avx512f",  "sse4_1", "sse4_2", "ssse3"};

	std::vector<std::string> cpuinfoFlags = split_string_to_vector(theInfo.processor_flags_full.c_str());
	std::sort(cpuinfoFlags.begin(), cpuinfoFlags.end());

	std::vector<std::string> interestingDetectedFlags;
	std::set_intersection(interestingFlags.begin(), interestingFlags.end(),
						      cpuinfoFlags.begin(),     cpuinfoFlags.end(),
						  std::back_inserter(interestingDetectedFlags));

	// These are the cpu processor flags that we care to advertise
    theInfo.processor_flags     = join_vector_to_string(interestingDetectedFlags);

	// Now compute the x86_64 microarch version
	theInfo.processor_microarch = cpuflags_to_microarch_string(cpuinfoFlags);

#endif
	theInfo.initialized = true;

    return &theInfo;
}
