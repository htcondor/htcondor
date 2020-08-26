
#define MAXCHANGES 10
#define STATUSSPACE 3000
#define K 1024
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
int getpid() { return (int)GetCurrentProcessId(); }
void sleep(int sec) { Sleep(sec * 1000); }
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")
#else
#include <sys/mman.h>
#include <errno.h>

#include <errno.h>
#include <unistd.h> // for sleep and getpid
#endif

#ifdef Darwin
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach/bootstrap.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#endif

	struct sizerequest {
		int memtimeatsize;
		int memsize;
		void *memchunk;
	};

	struct sizerequest timesteps[MAXCHANGES];
	int chunksize;
	void *stack[1000];
	int stackpointer;
	static char *status = NULL;
	int jobpid;
	int totalchunks;
	int totalK = 0;
	int memdiff = 0;
	int chunkK;

	void push(int chunks);
	void pop(int chunks);
	void init_storage(int size);
	const char *stamp();
	void report();

	void get_mem_data(unsigned int *vmpeak, unsigned int *vmsize, unsigned int *vmhwm, unsigned int *vmrss);

	int match_prefix(const char *s1, const char *s2);

	// Things grabbed from the HTCondor sysapi.h file
	// return values of different procapi calls
	enum {
		// general success
		PROCAPI_SUCCESS,
	
		// general failure
		PROCAPI_FAILURE
	};
	
	// status about various calls to procapi.
enum {
	// for when everything worked as desired
	PROCAPI_OK,

	// when you ask procapi for a pid family, and absolutely nothing is found
	PROCAPI_FAMILY_NONE,

	// when the daddy pid is included in the returned family
	PROCAPI_FAMILY_ALL,

	// when the daddy pid is NOT found, but others known to be its child are.
	PROCAPI_FAMILY_SOME,

	// failure due to nonexistance of pid
	PROCAPI_NOPID,

	// failure due to permissions
	PROCAPI_PERM,

	// sometimes a kernel simply screws up and gives us back garbage
	PROCAPI_GARBLED,

	// an error happened, but we didn't specify exactly what it was
	PROCAPI_UNSPECIFIED,

	// the process is definitely alive
	PROCAPI_ALIVE,

	// the process is definitely dead
	PROCAPI_DEAD,

	// it is uncertain whether the process is 
	// alive or dead
	PROCAPI_UNCERTAIN
};


/** This type serves to hold an opaque value representing a process's time
 *     of creation. It is platform-dependent since it holds whatever value the
 *         kernel gives us AS IS, so we can avoid rounding effects when using these
 *             values in comparisons to see if two processes share a birthday.
 *             */
#if defined(WIN32)
typedef __int64 birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%I64d"
#elif defined(LINUX)
typedef unsigned long long birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%llu"
#else
typedef long birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%ld"
#endif



/*
 * This gets us to an argc of 21
*/
int main(int argc,char **argv)
{
	//int szchng;
	int myarg;
	//char *pint;
	//void *got;
	struct sizerequest *request ;
	//FILE *out, *fopen();
	//const char *name = "mem_checker.pid";
	//const char *mode = "w";
	int chunkdiff;

	if(argc > ((2 * MAXCHANGES) + 1)) {
		printf("We can conly handle MAXCHANGES pairs\n");
		exit(0);
	}
	stackpointer = 0;
	status = malloc((size_t)STATUSSPACE);

	jobpid = getpid();
	//printf("my pid %d\n",jobpid);
	/*printf("my pid %d\n",getpid());*/

	init_storage(MAXCHANGES);
	/*
	display_storage(MAXCHANGES);
	*/
	chunkK = atoi(argv[1]);
	chunksize = ( K * chunkK);
	if(chunksize <= 0) {
		printf("Units of K must be greater then 0\n");
		exit(1);
	}

	//printf("Chunk size %d\n",chunksize);

	for(myarg = 2; myarg < argc; myarg++) {
		//printf("%s\n",argv[myarg]);
		myarg++;
		//printf("%s\n",argv[myarg]);
		/*printf("%d\n",atoi(argv[myarg]));*/
	}

	//printf("Storing requests\n");

	request = timesteps;

	for(myarg = 2; myarg < argc; myarg++) {
		request->memsize = atoi(argv[myarg]);
		/*
		printf("stored time %s\n",argv[myarg]);
		*/
		myarg++;
		request->memtimeatsize = atoi(argv[myarg]);
		/*
		printf("stored size %s\n",argv[myarg]);
		*/
		request++;
	}

	/*
	display_storage(MAXCHANGES);
	*/

	request = timesteps;
	totalchunks = 0;

	for(myarg = 0; myarg < MAXCHANGES; myarg++) {
		if(request->memsize == 0) {
			report();
			printf("done\nBye\n");
			exit(0);
		} else {
			//printf("%s time = %d size = %d - ",stamp(), request->memtimeatsize,request->memsize);
			if(request->memsize > 0) {
				if(request->memsize > totalchunks) {
					//printf("grow\n");
					chunkdiff = request->memsize - totalchunks;
					totalchunks = request->memsize;
					push(chunkdiff); /* grow */
				} else if(totalchunks > request->memsize) {
					//printf("shrink\n");
					chunkdiff = (totalchunks - request->memsize);
					totalchunks = request->memsize;
					pop(chunkdiff); /* shrink */
				} else {
					/* equal so do nothing */
				}
				totalK = chunkK * totalchunks;
				report();
			sleep(request->memtimeatsize);
			}
			request++;
		}
	}
	return 0;
}

void report()
{
	unsigned int vmpeak=-1, vmsize=-1, vmhwm=-1, vmrss=-1;

	get_mem_data(&vmpeak, &vmsize, &vmhwm, &vmrss);

	memdiff = vmrss - totalK;
	printf("%s PID %d, Alloc %d,  ",stamp(),jobpid,totalK);
	printf("VmPeak %6u, VmSize %6u, VmHWM %6u, VmRSS %6u, alloc diff %d kB\n", vmpeak, vmsize, vmhwm, vmrss, memdiff);
}

#if defined(WIN32)
/*
 * stackpointer always on location which can be filled
 */

void push(int chunks) {
	int units;

	//printf("push %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
		void * p = malloc(chunksize);
		if ( ! p) {
			printf("malloc of %d bytes failed!\n", chunksize);
			exit(1);
		}
		memset(p,units,chunksize);
		stack[stackpointer] = p;
		if(stackpointer != 999) {
			stackpointer++;
		} else {
			printf("exceeded the size of the stack\n");
			exit(1);
		}
	}
}

void pop(int chunks) {
	int units;

	//printf("pop %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
		void * p;
		if(stackpointer != 0) {
			stackpointer--;
		} else {
			printf("Tried to pop empty stack");
			exit(1);
		}
		p = stack[stackpointer];
		stack[stackpointer] = NULL;
		if (p) free(p);
	}
}

#else
/*
 * stackpointer always on location which can be filled
 */

void push(int chunks) {
	int units;

	//printf("push %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
#if defined(Darwin)
		stack[stackpointer] = mmap(NULL,(size_t) chunksize, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
		stack[stackpointer] = mmap(NULL,(size_t) chunksize, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
		if(stack[stackpointer] == MAP_FAILED) {
			switch (errno) {
				case EACCES:
						printf("Failed EACCES\n");
						break;
				case EAGAIN:
						printf("Failed EAGAIN\n");
						break;
				case EBADF:
						printf("Failed EBADF\n");
						break;
				case EINVAL:
						printf("Failed EINVAL\n");
						break;
				case ENFILE:
						printf("Failed ENFILE\n");
						break;
				case ENODEV:
						printf("Failed ENODEV\n");
						break;
				case ENOMEM:
						printf("Failed ENOMEM\n");
						break;
				case EPERM:
						printf("Failed EPERM\n");
						break;
				//case ETXTBSY:
						//printf("Failed ETXTBSY\n");
						//break;
			}
		}
		//printf("Push got us address %u\n",(unsigned int)stack[stackpointer]);
		memset(stack[stackpointer],units,(size_t) chunksize);
		if(stackpointer != 999) {
			stackpointer++;
		} else {
			printf("exceeded the size of the stack\n");
			exit(1);
		}
	}
}

void pop(int chunks) {
	int units;

	printf("pop %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
		if(stackpointer != 0) {
			stackpointer--;
		} else {
			printf("Tried to pop empty stack\n");
			exit(1);
		}
		munmap(stack[stackpointer],(size_t) chunksize);
	}
}

#endif

void init_storage(int size) {
	int szchng;
	struct sizerequest *request ;

	request = timesteps;

	for( szchng = 0; szchng < size; szchng++) {
		request->memtimeatsize = 0;
		request->memsize = 0;
		request->memchunk = NULL;
		request++;
	}

}

void display_storage(int size) {
	int szchng;
	struct sizerequest *request ;

	request = timesteps;

	for( szchng = 0; szchng < size; szchng++) {
		printf("Time %d ",request->memtimeatsize);
		printf("size %d\n",request->memsize);
		request++;
	}

}

const char *stamp()
{
	static char timebuf[80];
	time_t now;
			
	time(&now);
	strftime(timebuf, 80, "%m/%d/%y %H:%M:%S ", localtime(&now));
	return timebuf;
}

int
match_prefix(const char *s1, const char *s2)
{
    size_t  s1l = strlen(s1);
    size_t  s2l = strlen(s2);
    size_t min = (s1l < s2l) ? s1l : s2l;

	/* return true if the strings match (i.e., strcmp() returns 0) */
	if (strncmp(s1, s2, min) == 0)
		return 1;
 
	return 0;
}

//unsigned int vmpeak=-1, vmsize=-1, vmhwm=-1, vmrss=-1;

#if defined(WIN32)
void
get_mem_data(unsigned int *vmpeak, unsigned int *vmsize, unsigned int *vmhwm, unsigned int *vmrss)
{
	PROCESS_MEMORY_COUNTERS_EX mem;
	ZeroMemory(&mem, sizeof(mem)); mem.cb = sizeof(mem);
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
		*vmpeak = (int)(mem.PeakPagefileUsage /  1024);
		*vmsize = (int)(mem.PrivateUsage /  1024);
		*vmhwm = (int)(mem.PeakWorkingSetSize / 1024);
		*vmrss = (int)(mem.WorkingSetSize / 1024);
	}
}
#endif

#if defined(LINUX)
void
get_mem_data(unsigned int *vmpeak, unsigned int *vmsize, unsigned int *vmhwm, unsigned int *vmrss)
{
	FILE *fp;
	fp = fopen("/proc/self/status","r");
	if(!fp) {
		printf("Failed to open /proc/self/status\n");
	} else {
		printf("%s PID %d, ",stamp(),jobpid);
		while (fgets(status, STATUSSPACE, fp)) {
	    	char label[32]; int size;
	    	int items = sscanf(status, "%s %d", label, &size);
            	if (items == 2) {
					//printf("scan `%s` found %d items: '%s' %d\n", status, items, label, size);
					if (match_prefix(label,"VmPeak")) *vmpeak = size;
					else if (match_prefix(label, "VmSize")) *vmsize = size;
					else if (match_prefix(label, "VmHWM")) *vmhwm = size;
					else if (match_prefix(label, "VmRSS")) *vmrss = size;
            	} else {
					//printf("scan `%s` expetc 2 entities\n", status);
				}
		}
		fclose(fp);

		fp = fopen("/proc/self/smaps","r");
		if(!fp) {
			printf("Failed to open /proc/self/smaps\n");
		} else {
			printf("%s\n",stamp());
			while (fgets(status, STATUSSPACE, fp)) {
				printf("smaps: %s",status);
			}
			fclose(fp);
		}
	}
}
#endif

#if defined(Darwin)

/*
 *   Special structure for holding the raw, unchecked, unconverted,
 *     values returned by the OS when inquiring about a process.
 *       The range, units and in some cases even the type of each field 
 *         are determined by the OS.
 *         */
typedef struct procInfoRaw{

        // Virtual size and working set size
        // are reported as 64-bit quantities
        // on Windows
#ifndef WIN32
        unsigned long imgsize;
        unsigned long rssize;
#else
        __int64 imgsize;
        __int64 rssize;
#endif
        //
#if HAVE_PSS
        unsigned long pssize;
        bool pssize_available;
#endif
        
        long minfault;
        long majfault;
        pid_t pid;
        pid_t ppid;
#if !defined(WIN32)
        uid_t owner;
#endif
        
        // Times are different on Windows
#ifndef WIN32
        // some systems return these times
        // in a combination of 2 units
        // *_1 is always the larger units
        long user_time_1;
        long user_time_2;
        long sys_time_1;
        long sys_time_2;
        birthday_t creation_time;
        long sample_time;
#endif // not defined WIN32
        
        // Windows does it different
#ifdef WIN32 
        __int64 user_time;
        __int64 sys_time;
        __int64 creation_time;
        __int64 sample_time;
        __int64 object_frequency;
        __int64 cpu_time;
#endif //WIN32
        
        // special process flags for Linux
#ifdef LINUX
        unsigned long proc_flags;
#endif //LINUX
        }procInfoRaw;

procInfoRaw procRaw;

void
get_mem_data(unsigned int *vmpeak, unsigned int *vmsize, unsigned int *vmhwm, unsigned int *vmrss)
{
	int procstatus;
	procstatus = PROCAPI_OK;
 
	// clear memory for procRaw
	// initProcInfoRaw(procRaw);
	//printf("Entering Darwin only code\n");
 
	// set the sample time
	// Don't care procRaw.sample_time = secsSinceEpoch();
 
/* Collect the data from the system */
 
	// First, let's get the BSD task info for this stucture. This
	// will tell us things like the pid, ppid, etc. 
	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	task_port_t task;
	struct task_basic_info     ti;
	unsigned int count;
	size_t bufSize = 0;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = jobpid;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		if (errno == ESRCH) {
			// No such process
			procstatus = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			procstatus = PROCAPI_PERM;
		} else {
			procstatus = PROCAPI_UNSPECIFIED;
		}
		printf( "ProcAPI: sysctl() (pass 1) on pid %d failed with %d(%s)\n",
			jobpid, errno, strerror(errno) );

  		//return PROCAPI_FAILURE;
	} else {
		//printf("sysctl worked..........\n");
	}

  
	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);
	if (kp == NULL) { 
		printf("ProcAPI: getProcInfo() Out of memory!\n");
	} else {
		//printf("Got memory for further calls to sysctl\n");
	}

 
 
	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0) {
		if (errno == ESRCH) {
			// No such process
			procstatus = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			procstatus = PROCAPI_PERM;
		} else {
			procstatus = PROCAPI_UNSPECIFIED;
		}
		printf( "ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			jobpid, errno, strerror(errno) );
  
		free(kp);
  
		//return PROCAPI_FAILURE;
	} else {
		//printf("Fetch of kproc stuff worked.\n");
	}

  	if ( bufSize == 0 ) {
  		procstatus = PROCAPI_NOPID;
  		printf( "ProcAPI: sysctl() (pass 2) on pid %d returned no data\n",
  			jobpid );
  		free(kp);
  	 	//return PROCAPI_FAILURE;
	}
  
  	// figure out the image,rss size and the sys/usr time for the process.
  	kern_return_t results;
  	results = task_for_pid(mach_task_self(), jobpid, &task);
  	
	if(results != KERN_SUCCESS) {
  		printf("Call to task_for_pid for pid %d worked\n",jobpid);
	} else {
  	
		/* We successfully got a mach port... */
  
  	
		count = TASK_BASIC_INFO_COUNT;
  	
		results = task_info(task, TASK_BASIC_INFO, (task_info_t)&ti,&count);
		if(results != KERN_SUCCESS) {
			procstatus = PROCAPI_UNSPECIFIED;
 
			printf( "ProcAPI: task_info() on pid %d failed with %d(%s)\n",
				jobpid, results, mach_error_string(results) );
 
			mach_port_deallocate(mach_task_self(), task);
			free(kp);
  
			//return PROCAPI_FAILURE;
	}
  
	// fill in the values we got from the kernel
	procRaw.imgsize = (unsigned long)ti.virtual_size;
	procRaw.rssize = ti.resident_size;
	procRaw.user_time_1 = ti.user_time.seconds;
	procRaw.user_time_2 = 0;
	procRaw.sys_time_1 = ti.system_time.seconds;
	procRaw.sys_time_2 = 0;

	*vmsize = (unsigned int)(procRaw.imgsize/1024);
	*vmpeak = 0;
	*vmhwm = 0;
	*vmrss = (unsigned int)(procRaw.rssize/1024);

	//printf("Imagesize = %u K(%u) RSS = %u K(%u)\n",(unsigned int)procRaw.imgsize,vmsize,(unsigned int)procRaw.rssize,vmrss);
  
	// clean up our port
	mach_port_deallocate(mach_task_self(), task);

	}
}
#endif
