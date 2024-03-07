/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* Calculate how many cpus a machine has using the various method each OS
	allows us */

#if defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/sysctl.h>
#endif

/* For Linux, the counting code is it's own chunk */
#if defined(LINUX)
static void ncpus_linux( int *num_cpus,int *num_hyperthread_cpus );
#endif

#ifdef WIN32
typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);
// The version if winnt.h that ships with Vs9 doesn't define SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
// so this code won't build.  We use the define for PROCESSOR_ARCHITECTURE_NEUTRAL which is nearby
// in the modern winnt.h as a way of detecting the missing structure definition and commenting out
// the relevant code.
#ifdef PROCESSOR_ARCHITECTURE_NEUTRAL
#define HAS_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
typedef BOOL (WINAPI *LPFN_GLPIX)(
	DWORD, // RelationshipType
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, // Buffer
	PDWORD); // ReturnedLength
#else
#pragma message("WARNING: PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX not defined in winnt.h - using WinXP compatible code to detect CPU cores.")
#pragma message("WARNING: This version of HTCondor may not correct detect > 16 CPUS.")
#endif
#endif

#ifdef WIN32
static DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;

    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}
#endif


static bool need_cpu_detection = true;

void
sysapi_detect_cpu_cores(int *num_cpus,int *num_hyperthread_cpus)
{
	need_cpu_detection = false;

	{
#if defined(WIN32)
		int coreCount = 0;
		int logicalCoreCount = 0;

		#ifdef HAS_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
		LPFN_GLPIX glpix = NULL;
		#endif
		LPFN_GLPI glpi = NULL;
		HMODULE hmod = GetModuleHandle(TEXT("kernel32"));
		if (hmod) {
		#ifdef HAS_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
			glpix = (LPFN_GLPIX) GetProcAddress(hmod, "GetLogicalProcessorInformationEx");
			if ( ! glpix)
		#endif
				glpi = (LPFN_GLPI) GetProcAddress(hmod, "GetLogicalProcessorInformation");
		}
		#ifdef HAS_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
		if (glpix)
		{
			// this code for Win7 or later.

			// call once to determine buffer size.
			DWORD cbInfo = 0;
			glpix(RelationProcessorCore, NULL, &cbInfo);
			if (!cbInfo || cbInfo > 10*1024*1024) { // 10Mb is way more than we ever expect to need...
				EXCEPT("Error: Failed to determine space for SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX : %d needed", cbInfo);
			}
			PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX infoBuf = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(cbInfo);
			if ( ! glpix(RelationProcessorCore, infoBuf, &cbInfo)) {
				EXCEPT("Error: Failed to call GetLogicalProcessorInformationEx: %d", GetLastError());
			}

			// got the processor info, now we decode it.
			// the return value is a set of structures that have a Size field telling us how
			// far to the next structure.
			PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ph = infoBuf;
			for (char * pi = (char*)infoBuf; pi < (char*)infoBuf + cbInfo; pi += ph->Size) {
				ph = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)pi;
				ASSERT(ph->Relationship == RelationProcessorCore);
				coreCount++;
				if (ph->Processor.Flags & LTP_PC_SMT) {
					for (unsigned ix = 0; ix < ph->Processor.GroupCount; ++ix) {
						logicalCoreCount += CountSetBits(ph->Processor.GroupMask[ix].Mask);
					}
				} else {
					// TJ: is this right? or should we be counting set bits to determine number of cores here?
					logicalCoreCount += 1;
				}
			}
			free(infoBuf);
		}
		else
		#endif
		if (glpi)
		{
			// this code for pre-win7
			DWORD returnLength = 0;
			int byteOffset = 0;
			PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
			PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL;

			bool done = false;
			while(!done)
			{
				DWORD rc = glpi(buffer, &returnLength);
				if(rc == FALSE)
				{
					if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
					{
						if(buffer)
							free(buffer);
						buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
						if(!buffer)
							EXCEPT("Error: Failed to allocate space for SYSTEM_LOGICAL_PROCESSOR_INFORMATION");
					}
					else
					{
						EXCEPT("Error: Failed to call GetLogicalProcessorInformation: %d", GetLastError());
					}
				}
				else
				{
					done = true;
					if (!buffer)
						EXCEPT("Error: Failed to get SYSTEM_LOGICAL_PROCESSOR_INFORMATION");
				}
			}

			info = buffer;

			coreCount = 0;
			logicalCoreCount = 0;
			while(byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
			{
				switch(info->Relationship)
				{
				case RelationProcessorCore:
					coreCount++;
					logicalCoreCount += CountSetBits(info->ProcessorMask);
					break;
				default:
					break;
				}

				byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
				info++;
			}

			free(buffer);
		}
		else
		{
			SYSTEM_INFO info;
			GetNativeSystemInfo(&info);
			coreCount = logicalCoreCount = info.dwNumberOfProcessors;
		}

		if( num_cpus ) *num_cpus = coreCount;
		if( num_hyperthread_cpus ) *num_hyperthread_cpus = logicalCoreCount;

#elif defined(LINUX)
	ncpus_linux(num_cpus,num_hyperthread_cpus );
#elif defined(CONDOR_FREEBSD)
	int mib[2], cpus;
	size_t len;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(cpus);
	sysctl(mib, 2, &cpus, &len, NULL, 0);
	if( num_cpus ) *num_cpus = cpus;
	if( num_hyperthread_cpus ) *num_hyperthread_cpus = cpus;
#elif defined(Darwin)
	int cpus;
	size_t len = sizeof(cpus);
	if( num_cpus ) {
		sysctlbyname("hw.physicalcpu_max", &cpus, &len, NULL, 0);
		*num_cpus = cpus;
	}
	if( num_hyperthread_cpus ) {
		sysctlbyname("hw.logicalcpu_max", &cpus, &len, NULL, 0);
		*num_hyperthread_cpus = cpus;
	}
#else
# error DO NOT KNOW HOW TO COMPUTE NUMBER OF CPUS ON THIS PLATFORM!
#endif
	}

}

void sysapi_ncpus_raw(int * pncpus, int * pnhyperthread_cpus) {
	const char *cores_from_env = getenv("OMP_NUM_THREADS"); // magic env var to constrain threads
	int env_cores = 0;
	if (cores_from_env) {
		env_cores = atoi(cores_from_env);
		if (env_cores > 0) {
			*pncpus = *pnhyperthread_cpus = env_cores;
			return;
		}
	}

	if (need_cpu_detection) {
		sysapi_detect_cpu_cores(&_sysapi_detected_phys_cpus, &_sysapi_detected_hyper_cpus);
	}
	if (pncpus) *pncpus = _sysapi_detected_phys_cpus;
	if (pnhyperthread_cpus) *pnhyperthread_cpus = _sysapi_detected_hyper_cpus;
}
// NOTE: we do NOT want to param for NUM_CPUS here, only the startd really wants to be lied to about the number of cpus...
void sysapi_ncpus(int * pncpus, int * pnhyperthread_cpus) {
	sysapi_ncpus_raw(pncpus, pnhyperthread_cpus);
}

/*
 * Linux specific CPU counting logic; uses /proc/cpuinfo.
 * 
 * The alpha linux port does not use "processor", nor does it provide
 * seperate entries on SMP machines.  In fact, for Alpha Linux,
 * there's another whole entry for "cpus detected" that just contains
 * the number we want.  So, on Alpha Linux we have to look for that,
 * and on Intel Linux, we just have to count the number of lines
 * containing "processor".
 * -Alpha/Linux wisdom added by Derek Wright on 12/7/99
 *
 * See the file "README-Linux.txt" for sample /proc/cpuinfo output
 *
*/

#if defined(LINUX)

typedef struct Processor_s
{
	int			processor;		/* "processr" / offset in array */
	int			physical_id;	/* "physical id" / -1 */
	int			core_id;		/* "core id" / -1 */
	int			cpu_cores;		/* "cpu cores" / -1 */
	int			siblings;		/* "siblings" / -1 */

	bool		have_flags;		/* "flags" line? */
	bool		flag_ht;		/*   HT flag set? */

	/* These are for counting the number of records with matching ID info
	 * We define a match as having the same physical and core IDs */
	int				match_count;		/* Number of matches */
	struct Processor_s *first_match, *next_match;
} Processor;

typedef struct
{
	Processor	*processors;
	int			num_processors;

	// Resulting info
	int			num_cpus;			/* Final # of CPUs detected */
	int			num_hthreads;		/* # of "extra" hyper-threads */

	// Global info when available
	int			cpus_detected;		/* value: "cpus detected" / -1 */
	bool		have_siblings;		/* value: "siblings" / -1 */
	bool		have_physical_id;	/* Did we find a "physical id"? */
	bool		have_core_id;		/* Did we find a "core id"? */
	bool		have_cores;			/* Did we find a "cores"? */

	bool		have_flags;			/* "flags" line? */
	bool		flag_ht;			/*   HT flag set? */

} CpuInfo;

static int
my_atoi( const char *str, int def )
{
	if ( !str ) {
		return def;
	}
	else if ( !isdigit(*str) ) {
		dprintf( D_ALWAYS, "Unable to parse '%s' as an integer\n", str );
		return def;
	}
	return atoi( str );
}

/* For testing, this can be set to an open file with proc cpuinfo output */
SysapiProcCpuinfo	_SysapiProcCpuinfo = {
	NULL, 0L, -1, -1, -1, 0
};

static int
read_proc_cpuinfo( CpuInfo	*cpuinfo )
{
	int			 cur_processor_num = 0;	/* Current processor # */
	Processor	*cur_processor = NULL;	/* Pointer to the current CPU */
	Processor	*array = NULL;			/* Array of processors */
	int			 array_size;			/* Currewnt size of the array */
	FILE        *fp;					/* File pointer */
	/* Between the Xeon E5620 (Mar 16, 2010) and the E5-2470 (May 14, 2012),
	   the 'flags' line grew from 405 to 435 characters long.  At that rate,
	   we have 23 years before we need to increase the size of these buffers
	   again. */
	char 		 buf[1024];				/* Input line buffer */
	char		 buf_copy[1024];		/* Copy of the above */
	int			 errors = 0;			/* # of errors encountered */

	/* Initialize processor array chunks */
	cpuinfo->processors = NULL;
	cpuinfo->num_processors = 0;

	/* Initialize the global chunks */
	cpuinfo->num_cpus = 0;
	cpuinfo->num_hthreads = 0;
	cpuinfo->cpus_detected = -1;
	cpuinfo->have_siblings = false;
	cpuinfo->have_physical_id = false;
	cpuinfo->have_core_id = false;
	cpuinfo->have_cores = false;
	cpuinfo->have_flags = false;
	cpuinfo->flag_ht = false;

	/* Allocate the array to hold 8 to start with; we'll realloc() it
	 * bigger if we find more cpus
	 */
	array_size			= 32;
	array = (Processor *) malloc( array_size * sizeof(Processor) );

	/* Did we get the array we need? */
	if ( NULL == array ) {
		return -1;
	}

	/* Now, let's go through and gather info from /proc/cpuinfo to the array */
	if ( _SysapiProcCpuinfo.file ) {
		fp = safe_fopen_wrapper_follow( _SysapiProcCpuinfo.file, "r", 0644 );
		if( !fp ) {
			free( array );
			return -1;
		}
		int ret = fseek( fp, _SysapiProcCpuinfo.offset, SEEK_SET );
		if (ret < 0) {
			free( array );
			return -1;
		}
		dprintf( D_LOAD,
				 "Reading from %s, offset %ld\n",
				 _SysapiProcCpuinfo.file, _SysapiProcCpuinfo.offset );
	}
	else {
		fp = safe_fopen_wrapper_follow( "/proc/cpuinfo", "r", 0644 );
		dprintf( D_LOAD, "Reading from /proc/cpuinfo\n" );
	}
	if( !fp ) {
		free( array );
		return -1;
	}

	/* Look at each line */
	while( fgets( buf, sizeof(buf)-1, fp) ) {
		char		*tmp;
		const char	*attr;
		char		*colon;
		char		*value = NULL;

		/* Clip the buffer */
		buf[sizeof(buf)-1] = '\0';

		if ( strlen(buf) >= 1 ) {
			tmp = (buf + strlen(buf) - 1);
			while( isspace(*tmp) && (tmp != buf) ) {
				*tmp = '\0';
				tmp--;
			}
		}

		strcpy( buf_copy, buf );
		attr = buf_copy;
		colon = strchr( buf_copy, ':' );

		/* Empty line ends the current processor */
		if( strlen( buf ) < 2 ) {
			if( _SysapiProcCpuinfo.debug && cur_processor ) {
				dprintf( D_FULLDEBUG,
						 "Processor #%-3d:  "
						 "Proc#:%-3d PhysID:%-3d CoreID:%-3d "
						 "Sibs:%d Cores:%-3d\n",
						 cur_processor_num,
						 cur_processor->processor,
						 cur_processor->physical_id,
						 cur_processor->core_id,
						 cur_processor->siblings,
						 cur_processor->cpu_cores );
			}
			cur_processor = NULL;
		}

		/* Pull out the value, remove the whitespace from the attr */
		if ( colon ) {
			if ( *(colon+1) != '\0' ) {
				value = colon + 2;
			}
			tmp = colon;
			while( isspace( *tmp ) || (*tmp==':') ) {
				*tmp = '\0';
				tmp--;
			}
		}

		/* End of debug file */
		if( _SysapiProcCpuinfo.file ) {
			if ( !strncmp( attr, "END", 3 )  )
				break;
		}

		if(  ( !cur_processor ) &&
			 ( !strcmp( attr, "processor") || !strcmp( attr, "cpu" ) )  ) {

			/* If the array is no longer big enough, grow it */
			if ( cur_processor_num >= array_size ) {
				int			 new_size = array_size * 2;
				Processor	*new_array;

				dprintf( D_FULLDEBUG, "Growing processor array to %d\n",
						 new_size );
				new_array = (Processor *)
					realloc( array, new_size * sizeof(Processor) );
				if ( ! new_array ) {
					dprintf( D_ALWAYS, "Error growing processor array to %d\n",
							 new_size );
					EXCEPT( "Out of memory!" );
					break;
				}
				array = new_array;
				array_size = new_size;
			}

			/* Now, point at the new array element */
			cur_processor = &array[cur_processor_num];
			memset( cur_processor, 0, sizeof(Processor) );

			/* And, pull out the processor # */
			cur_processor->physical_id = -1;
			cur_processor->processor = cur_processor_num;
			cur_processor->core_id = -1;
			cur_processor->cpu_cores = -1;
			cur_processor->siblings = -1;

			cur_processor->have_flags = false;
			cur_processor->flag_ht = false;

			cur_processor->match_count = 1;			/* I match myself */
			cur_processor->first_match = NULL;
			cur_processor->next_match = NULL;

			/* Finally, update the count */
			cur_processor_num++;

		}

		if ( cur_processor ) {

			if ( !strcmp( attr, "processor") ) {
				cur_processor->processor = my_atoi( value, cur_processor_num );
			}

			else if( !strcmp( attr, "siblings" ) ) {
				cur_processor->siblings = my_atoi( value, 1 );
				cpuinfo->have_siblings = true;
			}

			else if( !strcmp( attr, "physical id" ) ) {
				cur_processor->physical_id = my_atoi( value, 1 );
				cpuinfo->have_physical_id = true;
			}

			else if( !strcmp( attr, "core id" ) ) {
				cur_processor->core_id = my_atoi( value, 1 );
				cpuinfo->have_core_id = true;
			}

			else if( !strcmp( attr, "cpu cores" ) ) {
				cur_processor->cpu_cores = my_atoi( value, 1 );
				cpuinfo->have_cores = true;
			}

			else if( !strcmp( attr, "flags" ) ) {
				cur_processor->have_flags = true;
				cur_processor->flag_ht = false;
				{
					char	*t, *save;
					t = strtok_r( value, " ", &save );
					while( t ) {
						if ( !strcmp( t, "ht" ) ) {
							cur_processor->flag_ht = true;
							break;
						}
						t = strtok_r( NULL, " ", &save );
					}
				}
				if ( ! cpuinfo->have_flags ) {
					cpuinfo->have_flags = true;
					cpuinfo->flag_ht = cur_processor->flag_ht;
				}
			}
		}

		if( !strcmp( attr, "cpus detected" ) ) {
			cpuinfo->cpus_detected =  my_atoi( value, -1 );
			if ( cpuinfo->cpus_detected < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Unrecognized format for "
						 "/proc/cpuinfo:\n(%s)\n",
						 buf );
				cpuinfo->cpus_detected = 1;
				errors++;
			}
		}
	}

	/* Done reading; close the file */
	fclose( fp );

	/* Store the info back into the struct passed in */
	cpuinfo->processors = array;
	cpuinfo->num_processors = cur_processor_num;

	if( _SysapiProcCpuinfo.debug ) {
		dprintf( D_ALWAYS,
				 "Processors detected = %d; CPUs detected = %d\n",
				 cpuinfo->num_processors, cpuinfo->cpus_detected );
	}

	return ( errors ? -1 : 0 );
}
	
/* For intel-ish CPUs, count the # of CPUs using the physical/core IDs */
static int
linux_count_cpus_id( CpuInfo *cpuinfo )
{
	int			pnum;					/* Current processor # */

	dprintf( D_LOAD,
			 "Analyzing %d processors using IDs...\n",
			 cpuinfo->num_processors );
		
	/* Loop through the processor records, find matching IDs */
	cpuinfo->num_cpus = 0;
	cpuinfo->num_hthreads = 0;
	for( pnum = 0;  pnum < cpuinfo->num_processors; pnum++ ) {

		/* Current processor record */
		Processor		*proc = &cpuinfo->processors[pnum];
		dprintf( D_LOAD | D_VERBOSE,
				 "Looking at processor #%d (PID:%d, CID:%d):\n",
				 pnum, proc->physical_id, proc->core_id );

		/* Temp processor pointer */
		Processor		*tproc;

		/* Number of matches */
		int				match_count		= 1;

		/* If we have a "first match" set, we've already been looked
		   at; Already done with me */
		if ( NULL != proc->first_match ) {
			continue;
		}

		/* If not already set, I'm the head of my chain */
		proc->first_match = proc;
		cpuinfo->num_cpus++;

		/* Look ahead through the list for matches */
		if (  ( proc->physical_id >= 0 ) || ( proc->core_id >= 0 )  ) {
			int		tpnum;	/* Temp PROC # */

			Processor	*prev_match = proc;		/* Previous match is myself */

			for( tpnum = pnum+1;  tpnum < cpuinfo->num_processors; tpnum++ ) {
				tproc = &cpuinfo->processors[tpnum];

				/* If it doesn't match, skip it */
				if (  ( ( proc->physical_id >= 0 ) &&
						( proc->physical_id != tproc->physical_id )  ) ||
					  ( ( proc->core_id >= 0 ) &&
						( proc->core_id != tproc->core_id ) )  ) {

					// Note this won't actually dprintf in the usual case where we are
					// called from a global ctor before main, because dprintf hasn't been
					// configured, but the shadow can't afford to allocate memory to dprint buffers

					if (IsDebugVerbose(D_LOAD)) {
						dprintf( D_LOAD | D_VERBOSE,
							 "Comparing P#%-3d and P#%-3d: "
							 "pid:%d!=%d or  cid:%d!=%d (match=No)\n",
							 pnum, tpnum,
							 proc->physical_id, tproc->physical_id,
							 proc->core_id, tproc->core_id );
					}
					continue;
				}

				/* Match! */
				prev_match->next_match = tproc;
				tproc->first_match = proc;
				match_count++;
				prev_match = tproc;
				cpuinfo->num_hthreads++;

				dprintf( D_LOAD | D_VERBOSE,
						 "Comparing P#%-3d and P#%-3d: "
						 "pid:%d==%d and cid:%d==%d (match=%d)\n",
						 pnum, tpnum,
						 proc->physical_id, tproc->physical_id,
						 proc->core_id, tproc->core_id, match_count );
			}
		}
		dprintf( D_LOAD | D_VERBOSE, "ncpus = %d\n", cpuinfo->num_cpus );

		/* Now, walk through the list of matches, store match count */
		for( tproc = proc; tproc != NULL;  tproc = tproc->next_match ) {
			tproc->match_count = match_count;
			dprintf( D_LOAD | D_VERBOSE, "P%d: match->%d\n",
					 tproc->processor, match_count );
		}
	}

	return 0;
}
	
/* For intel-ish CPUS, count the # of CPUs using siblings */
static int
linux_count_cpus_siblings( CpuInfo *cpuinfo )
{
	int		pnum;				/* Current processor # */
	int		np_siblings = 0;	/* Non-primary siblings */

	dprintf( D_FULLDEBUG,
			 "Analyzing %d processors using siblings\n",
			 cpuinfo->num_processors );
		
	/* Loop through the processor records, count sibling processors */
	cpuinfo->num_cpus = 0;
	cpuinfo->num_hthreads = 0;
	for( pnum = 0;  pnum < cpuinfo->num_processors; pnum++ ) {

		/* Current processor record */
		Processor		*proc = &cpuinfo->processors[pnum];
		
		/* If this one is a non-primary sibling, count it as such */
		if ( np_siblings > 1 ) {
			dprintf( D_FULLDEBUG,
					 "Processor %d: %d siblings (np_siblings %d >  0) [%s]\n",
					 pnum, proc->siblings, np_siblings, "not adding" );
			cpuinfo->num_hthreads++;
			np_siblings--;
		}
		else {
			dprintf( D_FULLDEBUG,
					 "Processor %d: %d siblings (np_siblings %d <= 0) [%s]\n",
					 pnum, proc->siblings, np_siblings, "adding" );
			cpuinfo->num_cpus++;
			np_siblings = proc->siblings;
		}
	}
	return 0;
}
	
/* For intel-ish CPUS, let's look at the number of processor records */
static int
linux_count_cpus( CpuInfo *cpuinfo )
{
	const	char	*ana_type = "";

	/* Did we find a "cpus detected" record?  If so, run with it. */
	if ( cpuinfo->cpus_detected > 0 ) {
		if ( cpuinfo->num_processors != cpuinfo->cpus_detected ) {
			dprintf( D_ALWAYS,
					 "\"cpus detected\" (%d) != processor records (%d); "
					 "using value from \"cpus detected\"\n",
					 cpuinfo->cpus_detected, cpuinfo->num_processors );
			cpuinfo->num_processors = cpuinfo->cpus_detected;
		}
	}
	
	/* Otherwise, let's go through and try to analyze what we have.
	 */
	dprintf( D_LOAD, "Found: Physical-IDs:%s; Core-IDs:%s\n",
			 cpuinfo->have_physical_id ? "True":"False",
			 cpuinfo->have_core_id ? "True":"False" );

	/* If we have physical ID or core ID, use that */
	if (  ( cpuinfo->flag_ht ) &&
		  ( cpuinfo->num_cpus <= 0 ) &&
		  ( cpuinfo->have_physical_id || cpuinfo->have_core_id )  ) {
		linux_count_cpus_id( cpuinfo );
		ana_type = "IDs";
	}

	/* Still no answer?  Try using the # of siblings */
	if (  ( cpuinfo->num_cpus <= 0 ) &&
		  ( cpuinfo->flag_ht ) &&
		  ( cpuinfo->have_siblings )  ) {
		linux_count_cpus_siblings( cpuinfo );
		ana_type = "siblings";
	}

	/* No HT flag?  Don't bother counting 'em */
	if ( cpuinfo->num_cpus <= 0 ) {
		cpuinfo->num_cpus = cpuinfo->num_processors;
		ana_type = "processor count";
	}

	/* Final sanity check -- make sure we return at least 1 CPU */
	if( cpuinfo->num_cpus <= 0 ) {
		dprintf( D_ALWAYS, "Unable to determine CPU count -- using 1\n" );
		ana_type = "none";
		cpuinfo->num_cpus = 1;
	}

	dprintf( D_CONFIG, "Using %s: %d processors, %d CPUs, %d HTs\n",
			 ana_type,
			 cpuinfo->num_processors,
			 cpuinfo->num_cpus,
			 cpuinfo->num_hthreads );

	return cpuinfo->num_cpus;
}


/* Linux specific code to count CPUs */
static void
ncpus_linux(int *num_cpus,int *num_hyperthread_cpus)
{
	CpuInfo		cpuinfo;		/* Info gathered from /proc/cpuinfo */

	/* Read /proc/cpuinfo */
	if ( read_proc_cpuinfo( &cpuinfo ) < 0 ) {
		dprintf( D_FULLDEBUG,
				 "Unable to read /proc/cpuinfo; assuming 1 CPU\n" );
		cpuinfo.num_cpus = 1;
	}
	/* Otherwise, count the CPUs */
	else {
		linux_count_cpus( &cpuinfo );
	}

	/* Free up the processors array */
	if ( cpuinfo.processors ) {
		free( cpuinfo.processors );
	}

	/* For debugging / testing */
	_SysapiProcCpuinfo.found_processors = cpuinfo.num_processors;
	_SysapiProcCpuinfo.found_hthreads = cpuinfo.num_hthreads;
	_SysapiProcCpuinfo.found_ncpus = cpuinfo.num_cpus;

	if( num_cpus ) {
		*num_cpus = cpuinfo.num_cpus;
	}
	if( num_hyperthread_cpus ) {
		*num_hyperthread_cpus = cpuinfo.num_processors;
	}
}

#endif		/* defined(LINUX) */

